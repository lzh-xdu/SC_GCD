# 作业 1：四模块输入输出规格

命名更新（2026-09-06）：按用户 CleanCode 要求，下面接口简称 tasks_in/tasks_out/results_in/results_out 在当前 C++ 中分别为 m_tasksIn/m_tasksOut/m_resultsIn/m_resultsOut，clk 为 m_clk；数据成员加 m_。语义和 CSV 名称保持不变，测试事件观察器现为 TestEventLog。

2026-09-06：先制定本规格，再按规格实现。以下具体 FIFO、时间戳、边界选择是本次 AI 实现方案，供用户审阅；题目要求与选择分开。

## 数据路径

Parser → parser_to_transform（有限 FIFO）→ Transform → transform_to_compute（有限 FIFO）→ Compute → compute_to_output（有限 FIFO）→ Output

所有模块使用同一个时钟的上升沿，SC_METHOD + dont_initialize；只调用非阻塞 FIFO 操作。不用 FIFO 事件唤醒硬件进程。
顶层负责文件参数、连线、统计与结束检测，不增加第五个处理模块。

## 类型与四模块接口

| 模块 | 输入 | 输出 | 内部状态和责任 |
|---|---|---|---|
| Parser | clk；输入文件流（仿真环境资源） | sc_fifo_out<RawTask> tasks_out | 每拍最多读取并发送一行；满时不读文件；EOF 后不再发送 |
| Transform | clk；sc_fifo_in<RawTask> tasks_in | sc_fifo_out<OrderedTask> tasks_out | 第一级取绝对值，第二级比较交换；两级各容纳一条；阻塞时保存状态 |
| Compute | clk；sc_fifo_in<OrderedTask> tasks_in | sc_fifo_out<Result> results_out | 空闲/计算中/结果待交付；单实例一次一条；按公式累计延迟 |
| Output | clk；sc_fifo_in<Result> results_in | 结果文件流（仿真环境资源） | 默认每拍最多接收一条；验证序号后每行写一个 GCD |

- RawTask：id(uint64)、a/b(int32)。id 是调试及保序验证标签，不进入功能输出。
- MagnitudeTask：id、a/b(uint64)，保存第一级的绝对值。
- OrderedTask：id、a/b(uint64)，满足 a>=b>=0。
- Result：id、gcd(uint64)。扩大类型以正确表示 abs(INT32_MIN)=2147483648；不更改输入范围。
- 三个 FIFO 默认深度均为 2，可统一配置为其他正整数；变换另有两个单条寄存器，计算另有一个任务/结果保存位置。结果文件不是硬件缓冲。

## FIFO 周期语义

选用 SystemC sc_fifo，单写者/单读者。在每个上升沿只依据该沿开始时可见的队列数据和空位操作。
本沿写入最早下一沿读取；本沿从满 FIFO 读出的空位最早下一沿被写者使用。不做同沿空队列直通或满队列旁路。
依据 sc_fifo 的 num_available/num_free 和 update 机制；这种保守规则与进程执行先后无关，但容量 1 时会降低链路吞吐。

## Transform 时序

第 k 沿成功取入原始任务并计算绝对值，保存在 stage1。
第 k+1 沿将旧 stage1 比较交换后存入 stage2。
第 k+2 沿将旧 stage2 写入输出 FIFO；无阻塞时模块接收到模块写出的延迟严格为 2T。
实现顺序为：尝试写旧 stage2 → 尝试将旧 stage1 移入空 stage2 → 尝试将新输入存入空 stage1。新输入不能同沿穿越两级。
两个寄存器各自有效/无效由 optional 表示。若 stage2 无法写出，保留它；stage1 有空间仍可接收，满后停止。
默认足够空间时可每拍接收/写出一条。发生背压时，2 周期基础延迟之外增加等待。

## Compute 时序与边界

对非零 b，沿 Euclidean 取余序列累加 max(1, bits(a)-bits(b)+1)。C++ 在接收沿计算数值和延迟，用计数器表达硬件忙碌时间；无需实现二进制除法内部电路。
在 k 沿接收，延迟 L>0 则 k+L 沿完成；完成沿可尝试写结果 FIFO，但绝不接收下一项。
若完成沿 FIFO 满，则保存结果，直到某后续沿写入；交付沿也不接新任务。这是简单实现的额外限制，不是题目强制。
若结果成功交付，下一沿才可接收新任务。下游读取结果最早在写入下一沿。

### Model Assumption：零输入与取模终止（spec 之外的约定）

2026-09-08：将原“零边界约定”显式展开，行为不变。以下是题目未明确规定时采用的模型假设，不是 spec 的硬性要求。

- 对取绝对值后的非负数 a、b，规定 `gcd(a, 0) = a`、`gcd(0, b) = b`、`gcd(0, 0) = 0`。对原始有符号输入，前两式分别为 `gcd(a, 0) = abs(a)`、`gcd(0, b) = abs(b)`。
- Transform 比较交换后满足 a≥b≥0，因此任一输入为零时，Compute 接收的 b 为零。
- 当 `b == 0` 时不执行 modulo operation（不计算 `% 0`），也不套用 modulo latency 公式 `max(1, bits(a)-bits(b)+1)`。此规则也适用于取余迭代中 b 变为零后的终止判断。
- 若接收时 b 就为零，取余次数为零，计算延迟 `L=0`；在接收沿完成并尝试写出，同沿最多接收这一条。输出阻塞等待及其他模块的延迟仍按各自规则计算，不能将 L=0 理解为整个系统零延迟。

实现依据（2026-09-09 更新路径）：原 Compute 内的 `while (b != 0)` 已抽取到共享 [延迟规划](../src/model/timing/gcd_plan.cpp)，仍同时保护取模和延迟累加；既有覆盖见 [边界测试](../tests/stage1/verify.py) 与 [极值输入集](../tests/stage1/cases/extreme_grid.txt)。2026-09-08 仅明确文档；本次结构重构与验证见 [Model 职责分层](model-structure.md)。

## Output 与文件规则

默认每拍最多输出一个结果；id 必须为 next_id，否则报告错误。功能文件仅为十进制 GCD，每行一个数。
可设置 output_period=N（每 N 拍接收一次）专用于下游阻塞测试，默认 N=1；配置进入统计与证据。
输入每一行严格要求两个有符号 32 位整数，允许空白但拒绝空行、额外字段、非法字符、越界；空文件合法。
文件错误或超时返回非零，失败产生的部分输出不得作为成功结果提交。

## 统计和结束

时钟 T=1 ns，首沿为 1 ns，沿编号从 1 开始；观察区间为 [0,结束沿*T]，总周期 C=结束沿编号，包含启动/排空/EOF 检测。
吞吐率=输出任务数/C；计算利用率=累计计算忙周期/C（零延迟任务贡献 0）。计算结果等待另计，不冒充有效计算。
记录各 FIFO 的容量、平均占用和峰值，两流水寄存器的平均/峰值占用；占用每沿更新后采样，对应下一周期的持续状态。初始及结束占用均为零，平均分母为 C。
顶层监测线程仅在上升沿后的 delta 采样统计（监测状态不参与硬件行为）；Parser EOF 且输出数=发送数且所有在途资源为空后停止。设最大模拟周期和 CTest 实际超时。
可选事件 CSV 单独输出 id,event,cycle,a,b,value,latency，记录模块接收/写出/完成；不能混入功能文件。

## 单任务时间线：48 18（默认配置）

| 沿 | 动作 |
|---|---|
| 1 | Parser 发送任务 0 |
| 2 | Transform 接收，stage1 取绝对值 |
| 3 | stage2 比较交换 |
| 4 | Transform 写出 |
| 5 | Compute 接收，L=6 |
| 11 | Compute 完成并写出 6，不接受新任务 |
| 12 | Output 接收并写文件，排空后结束 |

此表是待代码测试验证的规格预期，不能在运行前写成实际结果。

2026-09-09 命名补充：开头历史命名中的 TestEventLog 已由 EventRecorder 替代；数值和延迟实现已抽取，模块行为不变，见 [Model 分层](model-structure.md)。
