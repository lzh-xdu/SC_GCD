# 缺陷与主动测试案例

## B-008：旧慢输出断言不适用于预留窗口

- 2026-09-06，AI复用测试的假设错误，已修复。用户改为择闲+滑动窗口后，AI仍要求slow_sink的Compute结果等待必须大于0。
- 实际：新模型40条任务全部正确输出、520周期，握手阻塞352周期，但Compute结果等待为0，断言失败。窗口credit在派发处更早阻塞，未必堵在Compute结果端。
- 定位：核对新Dispatcher的窗口限制及stats，等待已转移到window_blocked，原断言不是题目功能要求。
- 修正：旧方案保留原断言，新模型检查窗口背压；另加宽输入FIFO、零延迟突发、结果FIFO=1的zero_burst，实际验证RESULT_PENDING路径，保留所有值/时序/容量检查。
- 原始日志evidence/stage3-window/initial-test-failure.log；最终Debug6/6及Release相关回归通过。并非处理器算术缺陷，不替代B-006主动案例。

## B-006：作业 3 最小结果容量造成额外计算停顿

- 日期2026-09-06；作业3主动设计缺陷案例，已通过容量配置改进。不是算术错误或死锁，不宣称所有负载均修复最优。
- 触发思路：严格轮转下短任务可能先完成但不能先输出；慢Output让结果继续积压。初版每路仅一个结果槽可能把已完成结果留在Compute，影响其下一次接收。
- 版本：75c76f5基础上的作业3首版未提交修改；输入tests/stage3/cases/capacity_pressure.txt，重复8次[长任务,(1,1),(2,2),(3,3),(4,4),(5,5)]，长任务为(1836311903,1134903170)。共48项，D=2、输出周期3，R先1后2。
- 复现：stage3_gcd INPUT OUTPUT STATS 2 EVENTS 3 1000000 1；最后参数改2为改进配置，改4为额外容量对照。
- 预期：值与顺序正确，并量化有限结果槽是否让计算资源无谓等待；没有预填失败周期。
- 实际R=1：663周期、结果等待74周期（Compute0=15、Compute1=59）。FIFO1平均占用约0.949/容量1，存在持续拥塞；事件显示晚编号先完成、结果交付延后。
- 定位：Compute的complete→emit差值累加等于等待计数；对应时段结果FIFO满，Collector被早编号或慢输出限制，ready持续低并反压Dispatcher。
- 改进R=2：648周期、等待7周期（Compute0=0、Compute1=7），总忙周期仍632，数值/顺序完全相同；多2个结果槽、192bit声明有效载荷。默认R从1改2，保留原参数可复现。
- R=4：648周期、等待0，额外资源未再改善此场景总周期，因此选择2作为默认折中。RR负载不均和保序等待仍存在，不靠换名字隐藏限制。
- 回归：tests/handshake/verify.py中capacity_before/after保持此对照，并检查真实乱序完成和保序最终输出；Release/Debug均通过，证据evidence/stage3/capacity-*。

## B-007：扩展握手检查时误缩进Python循环

- 日期2026-09-06，AI测试代码编辑错误，已修复；不作为作业3主动案例。
- 扩展ready容量检查时，顶层fixture循环意外多了四空格，两个握手CTest均以IndentationError停止；独立性能比较仍通过。
- 原始证据evidence/stage3/test-edit-failure.log；恢复顶层缩进，没有修改模型和断言规则。修正后Debug全5/5及Release相关回归通过。

## B-005：长任务测试把 FIFO 峰值断言写死

- 2026-09-06，作业 1 测试资产错误，已修正；AI 编写，不是处理器缺陷，也不属于作业 3/4 案例。
- 触发思路：连续相邻斐波那契输入应让上游有限 FIFO 填满，增加占用峰值断言。
- 基线 3db794c 加本轮未提交扩展，输入 cases/fibonacci_consecutive.txt；实际配置深度 4、输出周期 1。复现命令见 stage1-test-matrix.md。
- 预期：峰值等于配置容量；实际模型输出峰值 4，但 AI 断言错误要求 1，报 long tasks did not fill upstream FIFO；此前 GCD、时序和统计独立重算均通过。
- 定位：读取该场景 stats.csv 发现 capacity=4、peak=4，场景索引选择了深度 4；错误在测试常量。
- 修正：比较 peak 与 capacity，不改变模型、不放松为任意峰值。
- 原始失败与修正后 Release/Debug 各 2/2 回归见 evidence/stage1-expanded；本条所在提交保存代码和证据。

## B-004：测试前缀重命名误改第三方关键字参数

- 日期/阶段：2026-09-06，风格整理，已修复；不属于作业 3/4 案例。
- 场景：对 Python 黑盒测试的自定义变量统一 test_ 前缀后，执行既有回归。
- 实际：subprocess.run(..., test_text=True) 导致 TypeError，阶段 1 验证未开始；Day01 仍通过。
- 原因：标识符替换同时修改了标准库拥有的关键字参数 text，不是项目变量。
- 修正：恢复 text=True，保留自定义 test_text 变量；不修改测试判定或模型结果。
- 证据：evidence/style-debug/initial-test-failure.log；后续 Release 和 Debug 各 2/2 通过，见对应日志。

## B-003：AI 手写 GCD 预期错误

- 日期/阶段：2026-09-06，作业 1，已修正；不能代替作业 3/4 的主动案例。
- 触发思路：数值使用 math.gcd 独立参考，同时对照手写基本预期文件，以检查测试资产本身。
- 复现：./scripts/build-test.ps1；题目八组输入，深度 2、输出周期 1。
- 实际：数值参考与事件检查通过，但 published basic expected file 断言失败；手写第六项为 4，程序为 1。
- 证据：evidence/stage1/initial-test-failure.log；旧 master-plan.md 在 fdd3ecb 中也误写了 4。
- 定位：AI 初步怀疑末尾换行并尝试 splitlines，重试仍失败；逐项查看字符串后确认是数值差异。1024%17=4、17%4=1、4%1=0，因此 gcd=1。
- 原因：AI 把中间余数误当成最终 GCD，错误进入计划和手写 fixture；不是 Compute 算法失败。
- 修正：预期改为 1，撤回无关 splitlines 改动，计划保留显式更正记录；不修改算法以迎合错误预期。
- 回归：完整 CTest 2/2 通过，8 个成功场景及非法输入/超时检查通过，见 passing-tests.log。

## B-001：依赖下载 TLS 后端失败

- 类型：环境故障，已解决；历史补记自工作日志，不充当作业 3/4 缺陷案例。
- 触发：使用本机默认 Git HTTPS 后端下载官方 SystemC。
- 实际：SEC_E_NO_CREDENTIALS，下载失败。
- 处理：单次命令切换 OpenSSL 后端，不修改全局设置、不关闭证书校验。
- 验证：官方 3.0.1 固定提交下载成功，后续构建与测试通过。
- 限制：当时没有另存原始日志文件，错误输出来源为会话工具记录。

## B-002：CMake 包注册表权限警告

- 类型：环境配置问题，已解决；不是作业 3/4 案例。
- 实际：依赖配置尝试导出 Windows 用户包注册表，产生拒绝访问警告。
- 修正：设置 CMAKE_EXPORT_NO_PACKAGE_REGISTRY，继续使用工程内依赖。
- 验证：重新配置无该警告，增量构建及 CTest 通过。
- 证据：[CMakeLists.txt](../CMakeLists.txt)、工作日志。

## 阶段 3 主动场景：待设计与执行

状态更新2026-09-06：以下为原计划，实际已完成的场景为B-006（容量设计缺陷）；保留计划来源，不将设想本身算作证据。

- 候选思路：首任务慢、后续任务快，配合小结果缓冲，检查保序阻塞及是否出现死锁/覆盖。
- 原因：乱序完成和有限容量容易让正常输入未覆盖的路径暴露。
- 状态：仅为测试设想，无实现、无实际失败、无修正证据，不能计为满足要求。
- 后续：固定真实输入、延迟、容量和调度；执行后记录实际现象。若无问题，不虚构失败，继续审查并构造其他场景。

## 阶段 4 主动场景：待设计与执行

- 候选思路：任务完成与释放缓冲在同一仿真时刻发生，比较事件模型与第三阶段的接收/输出时间线，检查漏唤醒或提前接收。
- 原因：事件顺序和通知处理可能使边界行为不等价。
- 状态：仅为测试设想，尚无缺陷证据，不能计为满足要求。
- 后续：保存最小复现、两模型差异、原因、修正和回归结果。

## 2026-09-09：重构遗漏统计字段访问（已修正）

- 将 Compute 计数集中到 m_statistics 后，Release 编译暴露双实例报告仍按旧指针字段读取；首次构建退出 1。
- 定位、原始错误摘录、修正及两种构建回归证据见 [Model 分层验证](evidence/model-structure-validation.md)。另同步窗口占用观察的派发计数访问。
- 版本为基线 0df1381 加本轮重构，修复与记录同提交；这是 AI 重构编译缺陷，不计为硬件设计缺陷案例。

## B-009：Parser 事件迁移暴露日志比较过度约束

- 2026-09-10，作业 4 测试约束问题，已修正；不作为处理器真实缺陷案例计入阶段验收。
- 触发：将初版 Parser 的 SC_METHOD 改为无时钟 SC_THREAD 后，主动运行新旧等价对照；基线 1f252ab 加本轮 Parser/System 改动，Release，默认窗口 8/种子 1/FIFO 2，basic 输入。
- 预期：事件内容、时间点、结果和统计一致；原测试另外要求 CSV 全局行序相同。实际 basic/events.csv 断言失败（CTest 退出 8），同拍 parser_send 与 transform_accept 等行交换；29 个场景存在行序差异，其余结果与统计不变。
- 原因：独立进程的同拍执行顺序不是模块间因果协议；逐字节日志断言对初始复制适用，对进程迁移过度约束。
- 修正：Parser 事件流、剩余事件流分别严格比较，完整时间序列必须非递减；保留全部功能、握手、容量和统计断言。原始日志、两份 basic 事件及回归见 [验证记录](evidence/stage4-parser/validation.md)。
- 回归：Release/Debug 全量各 14/14；最终强化时间非递减检查后阶段 4 各 2/2。未宣称整个事件模型完成。

## B-010：事件调度诊断与旧日志整文件比较冲突

- 2026-09-10，测试适配问题，已修复；不计为作业 4 处理器缺陷案例。
- 触发：d78a477 加本轮全系统事件迁移后，Release 执行 `ctest --test-dir build -R stage4 --output-on-failure`。basic 默认配置结果/统计/事件均一致，原测试仍报告 basic/run.log 差异并退出 8。
- 实际额外内容为 `EVENT_SCHEDULER activations=33 max_jump_cycles=7`；该诊断描述实现开销，不属于硬件输出或时序语义。期望应为正式结果及原诊断一致，并单独核查新诊断。
- 修正只过滤新增诊断行参与旧日志比较，新边界测试明确解析并断言批次与跳跃；没有忽略正式结果、统计、事件时间或错误诊断。原始失败及最终双配置 15/15 证据见 [验证记录](evidence/stage4-events/validation.md)。

## B-011：详细 Trace 使事件模型更慢且内存增长

- 2026-09-10，作业 4 真实性能/内存设计问题，已复现、未修复；不是算术或时序错误，也不是日志断言适配问题。
- 触发思路：事件数减少未必意味着总仿真更快，主动用同一混合输入开关 Trace，同时测量耗时和峰值内存。
- 版本/配置：正式模型 cd03b91；本条提交的独立 profiling 包装入口。Release、GCC 13.2、SystemC 3.0.1、i5-12400F。输入为 Python Random(20260910) 在 int32 范围生成的前 1,000 对；FIFO/结果深度 2、窗口 8、仲裁种子 7、输出周期 1。
- 复现：构建 stage3_window_profile/stage4_profile 后，运行 `python scripts/compare_model_performance.py --cases mixed_small_off mixed_trace --report tmp/reproduce-b011`。预热各 1 次、串行交替各 7 次正式测量。
- 预期与实际：功能/系统时间/统计要求一致且实际满足。AI 性能假设为减少内核推进可能节省耗时；实际 Trace on 时事件入口耗时中位数 32.317 ms，高于时钟模型 25.518 ms（约慢 26.6%）；工作集 11.86 对 5.65 MiB。Trace off 同输入则为 5.388 对 12.276 ms（事件快 2.28×）。同样的 2.01 MiB 日志逐字节一致；delta 轮次 15,531 对 95,307，不能解释为更多模拟工作。
- 定位：stage4 EventRecorder::append 为行构造字符串并缓存在 vector，repeat 展开区间，flushTrace 使用 stable_sort 后写出；stage3 直接写流。这是与实测相符的设计解释，尚未分离各函数、分配和 I/O 的耗时贡献。
- 原始证据：[主实验样本](evidence/model-comparison/samples.jsonl)、[同输入控制样本](evidence/model-comparison/trace-controls/samples.jsonl)、[完整报告](model-comparison.md)；七次结果/统计哈希稳定。未制造功能失败或修改数据挑选最好一次。
- 修正建议（未实施）：改为按事件批次增量输出，或用区间表示长阻塞，在测试端归一化。需保留正确时间点、次数和顺序。
- 回归状态：本轮只测量并验证包装入口等价，原模型未改；Release CTest 15/15。未声称优化已完成，内存/耗时限制仍存在。

## B-012：simulated_time_ns 隐含周期为 1 ns

- 2026-09-10，用户审查指出；潜在单位缺陷，已修复。涉及五个入口，源码基线 9e613e9。
- 复现配置：真实 writeStats 报表方法，测试翻译单元周期时长 2.5 ns，System 已观察周期数 7；预期 simulated_time_ns=17.5、cycles=7，实际旧报表输出 simulated_time_ns=7。未运行非标准周期的完整仿真。
- 定位：写报表直接输出 elapsedSimulationCycles，未乘本阶段的周期时长。默认 1 ns 掩盖了问题。
- 修正：五个入口显式换算 ns，保留周期数及原字段名；时间限制等其他配置不在本轮扩展为任意周期支持。
- 回归：修复前五项报告测试失败，修复后两种构建全部 25/25；原始失败、方法和日志见 [四项修复](review-fixes.md)。

## B-013：超长正整数暴露库异常诊断

- 2026-09-10，用户审查指出；输入诊断缺陷，已修复。涉及五个入口，源码基线 9e613e9。
- 复现：30 个 9 作为正整数选项（测试 limit=100）；预期 option out of range，实际 std::stoull 在自定义校验前抛 out_of_range，最终显示 FAIL: stoull。输入仍被拒绝，不是越界值被接受。
- 修正：仅接受原有十进制字符，每次乘 10 和加 digit 之前按业务 limit 检查，无 uint64 溢出；不依赖 stoull 诊断，非法范围统一 runtime_error。
- 回归：30 位 9、0、101/limit=100 被拒绝；100、长前导零和 uint64 最大值正确处理；五个 CLI 实际返回友好信息。双配置 25/25，见 [证据](review-fixes.md)。
