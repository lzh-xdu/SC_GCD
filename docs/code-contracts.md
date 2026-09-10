# 命名与运行时契约整理（2026-09-07）

## 范围与决定

用户要求：各阶段 main.cpp 的 const 命名更准确，函数入口检查输入/环境并抛异常，改善头文件注释。
AI 实施选择：使用 `assertCondition(condition, message)`，以错误说明替代含义不明的布尔参数。
标准 C++17 `assert` 仅接受一个条件，失败终止进程，不抛 C++ 异常；NDEBUG 可以禁用它。
本项目辅助函数始终执行，默认抛 runtime_error；参数错误选 invalid_argument，内部不变量选 logic_error。
没有重定义 assert，也没有把该命名选择记作用户明确批准。

## 命名

四个现有 main.cpp（含 stage3_window）按对象、用途、单位命名，数值保持原值，例如：

| 原名 | 新名 | 含义 |
|---|---|---|
| DEFAULT_DEPTH | DEFAULT_FIFO_CAPACITY_TASKS | 默认外部 FIFO 任务容量 |
| QUEUE_COUNT | OBSERVED_FIFO_COUNT | 顶层纳入占用统计的 FIFO 数量 |
| DEFAULT_WINDOW | DEFAULT_REORDER_WINDOW_CAPACITY_TASKS | 结果重排窗口任务容量 |
| STATS_PRECISION | STATISTICS_SIGNIFICANT_DIGITS | 浮点统计输出有效数字位数 |
| FINAL_EDGE_MARGIN_NS | SIMULATION_STOP_MARGIN_AFTER_LAST_EDGE_NS | 停止时刻超出最后允许沿的裕量 |

命令行位置统一 ARGUMENT_INDEX 后缀；输出周期带 CYCLES；局部只读值使用 elapsedSimulationCycles、
handshakeValidCycles、fifoMetricNames、fifoCapacitiesTasks 等职责名称。const 不自动意味着配置常量。

## 函数检查范围

逐类审阅 src 中的函数，不采用“每个函数必须写一条恒真断言”的规则：

| 函数类别 | 检查位置与行为 |
|---|---|
| main 参数解析 | 入口核对 argc/argv、字符串指针和上限；解析后核对数值；sc_main 保留用法提示及异常捕获 |
| 路径、打开、刷新 | 保留路径互异保护；打开和刷新后核对操作结果，不能仅在入口判断未来 I/O 是否成功 |
| Parser / Output | 构造时核对借用流与输出周期；tick 检查流状态；读到任务后检查格式/顺序；EOF、FIFO 满/空正常返回 |
| Compute | gcdAndLatency 入口检查 a >= b；tick 检查 BUSY 剩余周期；deliver 检查结果就绪；accept 检查握手 |
| Transform | stage2 tick 检查 valid、排序槽与 data 一致；stage1 optional 容量由类型保证，无额外入口参数 |
| Dispatcher / Collector | 检查轮次索引、非零窗口/种子、窗口完成数；读到结果后核对编号、范围和槽冲突 |
| 统计、事件 | 非空事件指针、可写流、有效观测周期；事件边界顺序与未排空任务检查保留 |
| idle/empty/occupancy/drained、比较、bits/magnitude | 查询或全取值域合法的计算，没有额外可拒绝输入；零、负整数幅值及空任务集仍合法 |
| 模块连接、注册、观察循环 | 顶层私有调用路径受已检查的 parseOptions 保护；SystemC 负责端口绑定检查；不在构造期读取未绑定端口 |
| 流输出运算符、sc_trace、Day01 教学函数 | 保留标准流/SystemC 的既有接口约定和测试失败计数，不把正常流状态或第三方可接受参数改为异常 |

外部对象的寿命不能靠非空检查证明；头文件明确借用流、日志引用需存活到模块使用结束。
保留原有时序图和设计说明，补充参数单位、查询范围、调用条件与异常类型。
新增运行时检查可能增加主机执行开销，本轮未测主机性能；模型周期和吞吐测试通过不等于主机耗时不变。

## 验证

代码基线 `dab8ac8` 加本条所在提交的源码/CMake/测试修改。未包含用户原有 README.md 修改。

- `cmake --build build --parallel 4`：Release 构建退出 0。
- `ctest --test-dir build --output-on-failure`：12/12，退出 0。
- `cmake --build build/debug --parallel 4`：Debug 构建退出 0。
- `ctest --test-dir build/debug --output-on-failure`：12/12，退出 0。
- 原 6 项回归覆盖功能、时序、背压、顺序、性能对比及择闲窗口。
- 新 6 项测试：条件单次求值/异常类型、零输出周期、坏输入流、坏输出流、空事件指针、未完成及倒退统计事件。
- Release 编译数据库中的 contract_checks 命令含 `-DNDEBUG`，证明关闭标准断言时新增检查仍工作。
- clang-format 应用于本次源码；`git diff --check` 通过。
- 原始 CTest 输出：[Release](evidence/contracts-release.txt)、[Debug](evidence/contracts-debug.txt)。

这次属于现有阶段的可读性及错误诊断整理，不推进作业 4，不替代用户理解验收。
