# Model 职责分层

2026-09-09；用户提出按 Functional Behavior、Timing Behavior、Instrumentation 重构以利于阅读。
本轮应用于已实现的作业 1～3；不推进作业 4，也不改变现有模块数量和接口协议。

## 阅读入口与职责

| 层 | 实现入口 | 责任 |
|---|---|---|
| Functional Behavior | [operands.hpp](../../src/model/functional/operands.hpp) | 纯数值操作：安全取绝对值、比较交换、Euclidean 取余；无 SystemC、状态机或观测依赖 |
| Timing Behavior | [gcd_plan.hpp](../../src/model/timing/gcd_plan.hpp)、[clock.hpp](../../src/model/timing/clock.hpp) | 共享取余延迟策略、周期单位；一次遍历产生 GCD 和延迟，避免为数值与时序重复跑算法 |
| Timing Behavior：逐拍模块 | [FIFO Compute](../../src/stage1/compute.cpp)、[握手 Compute](../../src/stage2/compute.cpp) 及各阶段模块 | 继续负责接收、忙碌、完成、交付、握手、有限缓冲、背压、派发和保序 |
| Instrumentation / Trace | [event_recorder.hpp](../../src/model/instrumentation/event_recorder.hpp) | EventRecorder 统一观察入口；可选事件 CSV，原 TestEventLog 从任务类型头文件中移出并更名 |
| Instrumentation / Statistics | [statistics.hpp](../../src/model/instrumentation/statistics.hpp)、[task_statistics.hpp](../../src/common/task_statistics.hpp) | 模块计数器、占用采样、任务延迟；模块内计数统一归入 m_statistics |
| Instrumentation / Profiling | [profile_model.py](../../scripts/profile_model.py) | 外部进程耗时测量，报告独立 JSON；重复运行时核对功能和模拟统计一致 |
| Debug / 必需契约 | [contract.hpp](../../src/common/contract.hpp) | 输入有效性与内部不变量检查始终生效；不是可随日志关闭的可选功能 |

这是职责分层，不把各阶段目录机械搬成三个大目录，也不创建额外 SystemC 模块或继承体系。
各阶段 `main.cpp` 保留连线、排空停止与报告集成；共享计算与观测实现放入 `model_support` 库。
硬件信号、流水槽、Compute 剩余周期、Collector 标签和 Parser/Output 保序计数仍属于模型状态。
计数器不是控制依据；窗口未退休数量的观测可读取派发统计，但不会反馈到派发判定。

## 行为与约定

- 依赖方向：逐拍模块调用共享功能/延迟规划，并向 EventRecorder 报告；观测代码不能修改模型或驱动仲裁。
- `gcd_plan` 在非零除数时调用功能层取余并累加原公式；接受沿、完成沿与下一项接收规则不变。
- 零输入仍为 [Model Assumption](stage1-design.md)：跳过 `%0` 和该步延迟公式，零次取余 L=0。
- Trace 开关保持现有 `EVENTS.csv|-`，Statistics 仍启用；本轮没有实现全部 Instrumentation 的总关闭开关。
- Debug 中输入和状态契约始终执行，避免关闭观测后非法输入或损坏状态被静默接受。
- Profiling 记录整个主机进程耗时，含启动、模拟、观测和 I/O；不声称是纯模拟内核耗时，不写入模拟统计 CSV。

Profiling 示例（先准备输出目录）：

```powershell
python scripts/profile_model.py --report tmp/profile.json --repeat 5 -- build/stage1_gcd.exe tests/stage1/basic.txt tmp/output.txt tmp/stats.csv 2 -
```

## 阅读示例：同一任务的三个视角

2026-09-09 讲解补充。该树表示职责，不是任务依次流经三个新模块；实际数据路径仍为 Parser、Transform、Compute、Output（及作业 3 派发/收集）。

- 数值视角：48%18=12、18%12=6、12%6=0，最终 GCD=6。
- 时序视角：三步位宽公式分别为 2、2、2，共 L=6。作业 1 默认单任务接收沿为 5，完成/交付沿为 11，Output 读取沿为 12。
- C++ 可在沿 5 的 planGcd 调用中算出值和 L，但不推进 SystemC 时间；m_remaining 与状态机仍约束结果在沿 11 才能交付。
- Trace 记录接收、完成、交付各事件；Statistics 可汇总 12 个总周期、6 个计算忙周期及 50% 利用率。Profiling 另测主机进程实际耗时，与模拟的 12 ns 不等价。
- Debug 检查 BUSY 时剩余周期必须大于零等不变量；必要契约不是可随 Trace 关闭的观察功能。

区分状态与统计时，检查它是否参与下一步控制：m_remaining、m_state、m_nextId 属于模型状态；m_busyCycles、m_orderWaitCycles 属于观测统计。两者都可能是整数或计数器，不能按变量外形分类。
当前是职责分离而非“功能算法和延迟必须完全独立执行”：planGcd 在同一次 Euclidean 遍历中调用功能层取余并累计延迟，避免重复运算。

## 验证结果入口

基线 `0df1381`；变更与本说明同提交。重构前先运行 Release 12/12 回归并保存 372 份结果/统计/事件文件的 SHA-256。
新增 [Trace 开关等价检查](../../tests/instrumentation_equivalence.py)，在作业 1、2、3 轮转版与窗口版的每个有效场景中使用完全相同配置关闭 Trace，再逐字节比较功能输出和全部统计。
重构后构建、回归、基线对比与 Profiling 实测结论见 [验证记录](../evidence/model-structure-validation.md)。
