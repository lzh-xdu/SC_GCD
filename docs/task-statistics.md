# 统一任务延迟与阻塞原因

2026-09-07；适用于阶段 1、2、3 原轮转版及窗口版。只增加观察统计，不改变调度、容量和时序。

## 任务延迟

按任务 id 关联五个已有事件：Parser 发送 P、Compute 接收 A、计算完成 C、结果写入 FIFO E、最终 Output 接收 O。

| CSV 中 task_latency_ 后的名称 | 周期差 | 含义 |
|---|---|---|
| precompute | A−P | 计算前驻留：包含正常变换/传输与排队，不全是阻塞 |
| compute | C−A | 题目规定的计算耗时，可为 0 |
| result_wait | E−C | 计算完成后因结果 FIFO 不可写而滞留 |
| delivery | O−E | 结果 FIFO、收集、窗口保序和输出等待，包含正常传输 |
| end_to_end | O−P | 从进入模型到最终输出；不包含 Parser 尚未接纳前的文件等待 |

各项输出 `_sum_cycles`、`_mean_cycles`、`_max_cycles`、`_p95_cycles`；`task_latency_count` 为完成任务数。P95 使用排序后第 ceil(0.95N) 项；空输入计数为 0，四项统计约定填 0。

每条任务的四段之和等于端到端延迟，故总和/均值也可相加；最大值及 P95 不可相加。总执行周期仍从时间 0 计到排空结束，包含启动和 EOF 检测，因此不等于任务端到端延迟。

观察器实时消费已有事件，即使关闭详细 CSV 仍统计；只保留在途时间戳与完成任务的延迟样本，准确 P95 的主机内存为 O(N)，排序耗时 O(N log N)。这些不是硬件缓冲，不参与流控；阶段 4 公平计时须采用相同统计配置。

### 2026-09-09：Instrumentation 开关的实际验证范围

- 代码基线 `de1c0b3`：当前 `EVENTS.csv|-` 仅控制事件 CSV 写出，不是全部 Instrumentation 的总开关。`TestEventLog::record` 始终调用任务统计，模块和顶层统计也继续执行。
- [作业 1 测试](../tests/stage1/verify.py) 的 `without_events` 对 basic 输入分别开启/关闭事件 CSV，逐字节比较 `output.txt` 和 `stats.csv`。2026-09-09 上一轮 Release 12/12 回归包含此测试并通过；本轮为源码核查，未重新运行。
- [握手测试](../tests/handshake/verify.py) 覆盖作业 2、3 轮转版及窗口版的功能/时序，但尚未加入事件 CSV 开关的成对等价断言；不能把一般回归通过视为该项已验证。
- 当前不能宣称“全部 Instrumentation 开关在所有阶段均已验证一致”。主机运行时间也不要求一致：日志 I/O 和统计具有主机开销，但不应改变模型功能或模拟周期。

后续更新（2026-09-09，同日 Model 分层工作）：上述作业 2/3 的成对测试缺口已补齐；96 个有效场景现均对比 Trace 开/关的结果和完整统计，见 [分层验证](evidence/model-structure-validation.md)。观察器已更名为 EventRecorder。仍无全部 Instrumentation 总开关，不能把 Trace 等价扩大为统计完全关闭的等价验证。

## 关键原因及使用限制

| 指标 | 解释 |
|---|---|
| compute_busy_cycles（已有） | 有效计算工作量，不含等待；双单元求和可以超过系统周期 |
| compute0/1_idle_no_input_cycles（新增） | 沿开始为空闲但未接收任务；包括启动、供给不足、调度/窗口限制和排空，不一概判为缺陷 |
| compute_result_wait_cycles（已有） | 结果无法写出；等于所有任务 result_wait 的总和 |
| handshake_blocked_cycles（已有） | 有任务等待传输，但 ready=0；与计算忙或结果等待可能重叠 |
| dispatch_idle_other_blocked_cycles（原轮转版已有） | 被选单元无法接收，而另一单元空闲 |
| window_blocked_cycles（窗口版已有） | 上游有任务但窗口无信用；即使两个单元都忙也计数 |
| window_blocked_with_ready_cycles（窗口版新增） | 上一项的子集：窗口无信用且至少一个单元可接收，更能提示窗口限制了可用算力 |
| engines_blocked_cycles（窗口版已有） | 有窗口信用但两个计算单元均不可接收；与 window_blocked 互斥，两者之和等于握手阻塞 |
| reorder_wait_cycles / collector_output_blocked_cycles（已有） | 后续结果已到但队头未到 / 已可退休但输出 FIFO 满；区分保序和下游容量 |

新增空闲计数在 Compute 沿开始的状态上取样；窗口计数在派发沿读取握手前的 ready/信用。不能用同沿更新后的状态反推本沿是否本可接收。

阻塞计数不是消除该阻塞后必然节省的周期数。判断优化收益仍须修改一个因素，在同一输入/资源说明下实测总周期、任务延迟与资源代价。

## 少一拍与零延迟

单任务 `(48,18)` 的取余延迟始终为 6；默认配置：

| 事件 | 阶段 1 | 阶段 2 |
|---|---:|---:|
| Parser 发送 | 1 | 1 |
| Transform 接收/取绝对值 | 2 | 2 |
| 比较交换 | 3 | 3 |
| Transform 交付 | 4 | 4 |
| Compute 接收 | 5 | 4 |
| Compute 完成并写出 | 11 | 10 |
| Output 输出 | 12 | 11 |

阶段 1 在第 4 沿写 FIFO，Compute 只能在第 5 沿读；阶段 2 的数据/valid 在第 3 沿更新后已稳定，第 4 沿双方握手。减少的是中间 FIFO 的一个传输周期，没有缩短两级变换或 GCD 计算。慢输出等瓶颈可能掩盖这一个周期。

零延迟任务指绝对值/排序后 b=0，没有任何取余，如 `(20,0)`、`(0,-20)`、`(0,0)`。Compute 在接收沿完成这一条任务，但全系统仍需要解析、变换、传输和输出；同沿不能再接另一条。非零 `(30,30)` 需要一次取余，是 1 周期任务。

验证与示例数据见 [本轮证据](evidence/task-statistics/validation.md)。
