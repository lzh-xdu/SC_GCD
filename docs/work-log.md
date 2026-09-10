# 工作与 AI 协作记录

## 2026-09-08：作业3保序性能与资源分析

- 核对两版Collector、窗口Dispatcher及历史数据，新增[分析](archive/stage3-ordering-impact.md)，关联L-015/A-018。
- 明确队头阻塞、有限窗口背压、额外一拍、带宽及载荷资源，并区分综合架构收益和纯保序代价。
- 仅文档变更，核对引用与差异，不重跑无关代码测试；README既有修改不纳入。

## 2026-09-08：握手与零延迟代码讲解

- 对照当前 Transform/Compute 源码解释信号延迟更新、下一沿握手、零次取余同沿完成及 FIFO 满时保留结果。
- 新理解边界沉淀于 [学习记录 L-014](learning-log.md#l-014从代码追踪握手发布与零延迟交付)；未改代码，未重跑无关测试，保留 README 既有修改。

## 2026-09-07：最终设计入口与统一统计

- 用户授权实现优化审查第一项，关联D-017/A-017/L-013；[最终设计](final-design.md)简要归并硬约束、选择、优化目标及未完成项。
- 四模型新增统一延迟总和/均值/P95/最大值、Compute空闲无输入及窗口无信用但有ready；其余架构和参数不变。
- Debug/Release全6/6；96组前后输出、事件、旧指标一致，双构建统计一致；独立校验与辅助脚本真实失败修正见[证据](evidence/task-statistics/validation.md)。
- 下一步由用户理解统计及时间线，再按阶段推进。README既有改动未纳入；公开状态沿用待完成，未执行可见性变更。

## 2026-09-07：作业1～3优化空间审查

- 基线338534d；用户请求分析，关联A-016/L-012。现有模型和默认参数不改。
- 执行tests/stage3/optimization_probe.py build build/optimization-review，58组配置全部通过独立GCD检查；没有声称重新跑完整时序回归或已测主机加速。
- 报告覆盖硬性边界、FIFO气泡、窗口/R容量、头部旁路、指标可比性、测试和工程/主机改进，区分实测/建议及优先级。
- 证据evidence/optimization-review，报告stages1-3-optimization-review.md；用户README既有改动不纳入。

## 2026-09-06～07：择闲调度与滑动窗口性能对比

- 用户方案D-016/L-011/A-015；旧基线ad4b33e先运行32次并保留，后新增stage3_window独立目标，旧源码未改。
- 预留窗口防止最早结果无槽，保留单沿接收/退休带宽与Compute时序；完整任务id沿用，默认W=8/seed=1。
- 新68次性能运行：偏斜906→470、突发648→375、基本57→47；斐波那契560→561、首项长120→121，保留退化。旧R=6等载荷容量对照未消除轮转瓶颈。
- 最终Debug全6/6，Release相关5/5，作业1独立输出目录回归通过；新模型31场景和9类非法输入验证，统计跨构建一致。B-008首轮断言失败原始证据保留。
- 运行和证据见stage3-window.md、evidence/stage3-window；仅扩展VSCode入口，不声称GUI实测。用户README原改动未纳入。

## 2026-09-06：验证握手背压与性能瓶颈的区别

- 基线2cb6b27；用户观察与澄清见L-010/A-014。模型未改动，无需重复构建全部目标。
- 执行三次固定40条(30,30)、D=1对照：作业2快/慢Output为84/520周期，作业1慢Output亦520；检查实际输出均40行30。
- 记录握手451周期与结果等待425周期的重叠含义，不当成独立可相加损失；证据evidence/stage2-blocking。

## 2026-09-06：作业 3 双实例、保序及容量改进

- 先完成作业2提交75c76f5，再实现作业3，保留全部既有目标。用户指定严格顺序分配/处理；关联D-013～015、A-013、B-006/B-007。
- 新增Dispatcher与Collector独立文件/接口图、stage3_gcd、22场景握手/保序测试、6负载单/双模拟周期和资源比较；VSCode扩展2/3运行调试入口。
- 主动容量压力R=1→2总663→648周期，等待74→7；R=4无进一步总周期收益，默认选2。保留原始事件/数值及配置，没有虚构算术失败。
- 最终Debug全CTest 5/5；Release新阶段/比较与Day01 4/4，原阶段在独立输出目录24场景1434项通过；阶段2/3统计及比较表Release/Debug一致。复现见stage3-run.md及evidence/stage3。
- 构建/断言有实际执行证据，GUI调试入口仅配置验证；原README用户修改不纳入。作业4、公开访问、候选人全部代码理解仍待完成。

## 2026-09-06：作业 2 独立实现与验收

- 用户授权连续完成作业2、3，本工作单元先交付作业2；关联A-012/L-009。
- src/stage2 的握手流水/Compute、共享组件库、独立目标和19场景回归；新增阻塞和资源代理统计；保留作业1。
- Debug全CTest 3/3，Release作业2通过，原作业1汇总路径写入PermissionError后换独立目录完整通过；原始失败没有隐去，见stage2-run.md及evidence/stage2。
- 基本输入78周期、忙66、握手阻塞53周期；没有把少一拍解释为算术优化，也未声明候选人已完全理解。
- 下一工作单元：严格轮转双Compute、顺序结果处理、主动设计缺陷场景与资源/性能对比。

## 2026-09-06：补齐作业 1 测试及 6.6 审核

- 基线 3db794c；按用户要求先完善作业 1，不实施作业 2。关联 A-011、L-008、B-005。
- 已有独立功能/性能文件，本轮将自有 PASS 诊断移至 stderr，验证功能纯数字、追踪开关等价和路径冲突保护。
- 增加可复现的极值网格、连续相邻斐波那契、10×64 随机和短长混合输入，扩充至 14 类非法输入，保留实际输出/统计。
- 首轮断言错误及原始失败已留存，修复后 Debug/Release 各 CTest 2/2，24 有效场景/1434 任务；模拟指标汇总逐字节相同。命令及配置见 stage1-test-matrix.md。
- 原题 6.6 逐项核对见 engineering-audit.md：记录体系已有证据，仍缺公开仓库、完整理解验收、阶段 3/4 真实案例与模型效率比较。GitHub 元数据确认仍 private。
- 修正重复日志编号和远端过时状态，历史结论/失败不删除。用户既有 README.md 修改不纳入。

## 2026-09-06：进入作业 2 的接口澄清

- 用户询问阶段推进并给出接收判断；核对现有 Compute/Transform 实现后，说明显式握手与 FIFO 门控的区别。
- 新增 stage2-interface-proposal.md，关联 L-007/A-010；包含接口方向、接收/保持规则和待定周期边界。
- 本轮纯设计文档，无代码修改和新测试；作业 2 未实现。

## 2026-09-06：流水职责命名和逆序推进解释

- 用户审查意见见 A-009、D-011；理解与解释见 L-006。
- 重命名 Transform 两个内部槽，补充 namespace 与立即更新注释，更新观察指南；不改动算法。
- 保留头文件既有用户注释调整，README 独立修改不纳入。
- 验证：./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin，退出 0，CTest 2/2 通过。

## 2026-09-06：独立模块文件及注释接口图

- 基线 8009f6f，按用户要求拆分，关联 D-010、A-008。
- 新增四组模块 .hpp/.cpp 及 types.hpp/.cpp，删除旧 modules 文件；更新 main include、CMake 和当前阅读/断点路径。
- 每个头文件给出数据方向、上升沿触发、内外缓存、基础延迟/吞吐及背压行为；中文注释以 UTF-8 保存。
- 生成注释时发现本机 Python 默认编码导致中文显示异常，转换中又出现多余空行；改用显式 UTF-8 从原提交重新迁移并格式化，重新通过 Debug 回归。未改动算法。
- 验证命令：./scripts/build-test.ps1；./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin。两者各 2/2 通过。
- 没有改动算法与时序；历史证据不改写，README 既有用户修改不纳入提交。

## 2026-09-06：仿真观察与停止信息说明

- 用户报告停止信息并询问如何观察仿真；核对现有启动参数、两处 sc_stop 和本地内核消息来源。
- 新增 inspect-simulation.md，记录事件列含义、单任务时间线、调试入口及当前 Stage1 没有完整 VCD 的边界；关联 L-005。
- 仅文档更新，无新代码测试，不把该条 Info 当作故障或单独的成功证据。

## 2026-09-06：代码规范、测试前缀与 VS Code 调试

- 用户要求与反馈对应 D-008/D-009、A-007、L-004；读取指定 CleanCode 文件，应用于自有 C++ 源码。
- 基线：作业 1 提交 7558cfe，期间已存在 719360f IntelliSense 配置提交，保留并扩展；README 既有改动未纳入。
- 变更：测试前缀、成员命名、函数拆分、显式转换、大括号、常量、版权文档头；.clang-format；VS Code 五个 JSON；构建脚本支持独立 Debug。
- 真实失败 B-004 已保留：Python 第三方 text 参数误改，修正后 Release/Debug 各 2/2 通过。
- 验证：./scripts/build-test.ps1；./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin；JSON 解析及 clang-format --dry-run --Werror；C++ 行宽不超 120。
- 基本八组 Debug 统计与旧证据逐字一致，仍为 79 周期；未改变处理算法或时间约定。
- GDB 14.2 分别命中 Parser::tick/Testbench::run，读取计数器为 0，next 后继续正常退出；GUI F5 流程未实测。
- 状态：继续作业 1 用户审阅，不推进作业 2。

## 2026-09-06：作业 1 规格与首版实现

- 用户目标：先写四模块输入输出规格，再代码化；先产出 stage1-design.md，后实现 modules.hpp/.cpp 和 main.cpp。
- 基线 fdd3ecb；关联 D-007、A-006、B-003。原 README 的用户未提交修改未纳入本次提交。
- 接口：有限 FIFO；原始 int32、内部 uint64；k 到 k+2 变换；单计算延迟按公式；结果输出独立于统计/事件。
- 首次验证失败：AI 手写基本预期第六项错误；保留原始日志，修正后完整回归通过。阶段 1 案例不冒充阶段 3/4 案例。
- 最终命令 ./scripts/build-test.ps1，退出 0，2/2 CTest 通过；8 个成功场景累计 562 任务，另有非法输入及超时检查。
- 基本八组 79 周期、66 计算忙周期；单任务 12 周期时间线通过；容量 1/慢输出发生真实等待且无覆盖。
- 保存功能/统计/事件和失败/通过日志到 docs/evidence/stage1；运行说明见 stage1-run.md。
- 当前：第一版已实现和测试，尚待候选人逐模块审阅，不开始作业 2。

## 2026-09-06：完整计划与验收清单对齐

- 用户要求重新明确完整计划、阶段目标及原题注意事项。
- 新增 master-plan.md，区分原题硬性要求、实现建议、未定约定和未执行测试，保留四阶段完整范围。
- 修正预算表达：21 小时是初始总预算，已耗时间未统计，不能从本轮重新计满 21 小时。
- 当前仍为学习准备阶段，既有示例通过不等于作业 1 完成；详细计划未宣称用户已批准全部建议。
- 验证：核对已读取原题与现有状态/决策；纯文档变更，无新代码测试。

## 2026-09-06：SystemC 与电路实现的完整入门讲解

- 阶段：环境与学习；用户要求用 C++ 背景理解 SystemC 全貌和硬件生成路径。
- 修改：学习记录 [L-004b](learning-log.md#l-004b从-c-到-systemc再到电路)、AI 协作记录 [A-006b](ai-log.md#a-006bsystemc-全景教学与硬件生成边界)。
- 验证：核对本地 SystemC 3.0.1 调度/信号源码、现有示例和官方综合/物理实现资料；检查文档差异和链接。纯教学文档改动，未新增运行、时序、性能或综合通过结论。
- 范围：未推进作业实现，未改变工具链和待定时序约定。工作树原有 README.md 修改不属于本轮记录提交。

## 2026-09-06：八个 SystemC 调用的学习说明

- 用户要求：解释时钟构造、VCD 创建/单位/注册/关闭、sc_start、结束标记及 dont_initialize。
- 新增 docs/day01-api-guide.md，学习记录 L-003 链接该说明；没有修改代码。
- 验证方式：阅读本地 SystemC 3.0.1 头文件/内核源码及既有 VCD。未重新运行测试，不产生新的测试通过结论。
- 限制：结束阶段标记不代表测试全部执行；100 ns 模拟时间上限无法约束零时间死循环，现有 CTest 另有实际时间超时。

## 2026-09-06：采纳用户命名审查

- 基线：558afc9；阶段：第一天示例，非作业 1 完整实现。
- 用户提出端口/信号命名问题及 LLVM 综合疑问，分别记录 D-005、D-006、A-005、L-002。
- 修改：src/day01_basics.cpp 的端口、连接信号、波形名称；更新学习说明。
- 验证命令：./scripts/build-test.ps1 -Run；在包含本轮未提交重命名的工作树上执行，退出 0，CTest 1/1 通过，六个信号检查及时间检查通过，运行 PASS。
- 工具链：本轮仍为原 GCC，未进行 Clang 验证或综合验证。普通 C++ 编译器选择不自动提供硬件综合能力。
- 后续：继续明确作业 1 时序；若用户明确偏好 Clang，再独立迁移验证。

自 2026-09-06 起，此文件作为时间顺序摘要；详细内容见 [记录方式](process.md)。原始记录保留，补记不伪造历史提交。

## 2026-09-05：环境搭建

### 用户背景与目标

- Qt 框架鸿蒙化，C++ 三年经验；了解数字电路和并发。
- 每天三小时，一周约 21 小时。
- 岗位：GPU 架构（软件系统）研发工程师。
- 本次要求：开始第一天，在当前目录准备编译、运行、测试环境。

### 本次 AI 方案

- 复用本机 MinGW GCC、CMake 和 Ninja，避免另行搭建操作系统环境。
- 使用官方 SystemC 3.0.1 源码，与项目一起按 C++17 构建静态库。
- 添加两个寄存器的最小示例，以验证同沿旧值读取、更新阶段及 delta cycle。
- 使用显式检查和非零退出码，确保 Release 构建也能检测错误。
- 不提前实现完整作业，保留候选人理解并决定架构的步骤。

### 遇到的问题及处理

- Git 默认 Windows TLS 后端出现 SEC_E_NO_CREDENTIALS；对该次下载使用 OpenSSL 后端，未修改全局 Git 配置，未禁用证书校验。
- SystemC 配置阶段尝试写用户 CMake 包注册表而产生权限警告；关闭包注册表导出，依赖仅在当前构建中使用。

### 候选人待补充

- 自己对时序示例的解释。
- 对 AI 方案或代码的实际修改及理由。
- 自己发现的问题、证据与修正过程。

公开仓库发布尚未进行；学习完成情况不能由工具构建成功代替。

### 实际验证结果

- 官方版本提交校验通过。
- Windows MinGW Release 首次构建成功；修改包注册表配置后增量构建成功，注册表警告消失。
- CTest：1/1 通过；三次上升沿共六个信号值检查及 25 ns 时间检查通过。
- 独立运行示例返回 0，输出 PASS，生成 build/day01_basics.vcd。
- 首次依赖编译在 MinGW 的 Windows Fiber 内联头文件处出现数组边界警告；未修改或屏蔽依赖代码。当前基础测试通过，更广泛线程行为随后续模型继续验证。
- 本次测试覆盖基础仿真语义，不代表后续作业功能或性能已验证。

## 2026-09-06：记录体系与学习补记

- 用户要求：后续及时记录设计、调试、修改、验证和 AI 协作，跟踪公开 Git 与阶段 3/4 真实缺陷案例。
- 修改：增加根目录 AGENTS.md、process/status/decisions/learning-log/ai-log/defects 文档；更新 README 和 day01 学习状态。
- 补记来源：此前会话及现有文件；未精确归日的学习消息统一注明补记，不伪称当天实际实验。
- 关键结论：D-003 状态机仍是教学方案；D-004 两次采样沿不等于两个完整周期；L-001 记录用户的实际推演与纠正。
- 验证：本轮为文档变更，核对现有文件和对话，无新代码测试结果。既有 1/1 通过属于环境搭建阶段。
- 缺陷状态：B-001/002 为真实环境问题；阶段 3/4 仅有候选测试思路，尚未满足主动案例要求。
- 仓库状态：初次检查未初始化 Git，随后已初始化本地 main。用户提供 SC_GCD 地址；GitHub 连接确认该仓库为空、当前私有，公开要求尚未完成。
- 远端访问：本机 Git 凭据存储报错，匿名访问私有仓库失败；使用已连接 GitHub 成功核对仓库状态。已请用户改为公开，未修改全局凭据设置。
- 下一步：明确作业 1 时间线与有限缓冲规则，再实现并验证。

### Git 落地与验证

- 本地暂存差异通过 git diff --cached --check，共 15 个源码/脚本/文档文件；本轮无代码变更，不重复运行 CTest。
- 沙箱内 .git 写入受限；提升执行后遇到目录所有者检查，采用仅限本次命令的 safe.directory 指定当前项目路径。
- 在用户环境中认证推送成功；首次提交 `73407ee` 已同步 origin/main，没有强制推送或重写历史。
- 原题 PDF、third_party、build、tmp 和波形未进入提交。
- 远端当前检查为私有；内容同步完成不代表公开访问条件完成。已请用户调整可见性。

## 2026-09-06：接入 VS Code IntelliSense

- 目标：消除 `src/stage1/modules.hpp` 等文件因缺少 SystemC 编译上下文产生的预编译/语法解析错误。
- 修改：CMake 开启 `CMAKE_EXPORT_COMPILE_COMMANDS`；新增 `.vscode/settings.json` 和 `.vscode/extensions.json`，让 C/C++ 扩展复用真实 GCC/CMake 编译参数。协作详情见 [A-007b](ai-log.md#a-007b配置-vs-code-的-c-语法解析)。
- 验证：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Strawberry/c/bin/g++.exe`、`cmake --build build --parallel 4` 和 `ctest --test-dir build --output-on-failure` 均退出 0，CTest 2/2 通过；编译数据库确认 Stage 1 含 SystemC include、`SC_ENABLE_ASSERTIONS` 和 C++17。
- 范围：没有改变功能及时序；未包含用户已有的 `README.md` 修改。

## 2026-09-06：Stage 1 trace 终端可视化

- 用户请求以 Python 终端交互图形理解过程和效率；新增 scripts/trace_tui.py、针对性单元测试和[中文使用说明](archive/trace-viewer.md)，在 stage1-run.md 增加入口。
- 实际输入得到 8 任务、79 事件周期、66 忙周期、83.54% 利用率、结果阻塞 0；当前数据主要体现计算与上游积压，不能度量主机模拟速度。
- 5/5 工具测试通过；实际 Windows 伪终端导航、播放与退出通过；[验证及快照](evidence/stage1/trace-viewer-validation.md)。不修改 C++，不重复模型 CTest，不推进后续阶段。
- 协作记录 [A-008b](ai-log.md#a-008bpython-终端-trace-可视化)，学习记录 [L-006b](learning-log.md#l-006b用-trace-区分驻留计算与主机运行效率)。下一步由用户结合时间线审阅现有时序，尚未实施优化设计。

## 2026-09-06：解释 Day01 VCD 与查看方式

- 实读 build/debug/day01_basics.vcd、示例源码及本地 VCD tracing 源码；核对 GTKWave 官方文档，补充 [波形说明](archive/inspect-simulation.md) 和 [L-007b](learning-log.md#l-007bvcd-与实际波形末尾的区别)。
- 新观察：文件末尾 #25 无信号变化；保留事实，未将代码预期冒充波形记录，未实施末沿记录修复。
- 文档轮次，未安装查看器、未运行模型测试；本机 PATH 无 gtkwave。未触碰 README 原有改动。

## 2026-09-07：命名、函数校验与头文件注释

- 用户要求改善各阶段 const 命名、增加入口异常校验、优化头文件注释；AI 实施始终生效的 requireCondition，保留异常分类和诊断。
- 详细范围与取舍见 [命名与运行时契约](archive/code-contracts.md)，规范同步至 [coding-style.md](coding-style.md)。
- Release/Debug 构建退出 0，CTest 各 12/12：原 6 项回归及新增 6 项异常测试通过。证据链接见详细记录。
- 未推进作业 4；用户原有 README.md 改动不纳入本轮提交。未做主机执行开销基准。

## 2026-09-08：明确零边界模型假设

- 按用户要求，在 [阶段 1 设计](archive/stage1-design.md) 和 [最终设计](final-design.md) 显式标注 spec 外的零输入约定，保留原行为并注明本次澄清日期。
- 核对 Compute 的循环条件及既有边界输入；仅改文档，未重新运行模型测试。协作记录见 [AI 日志](ai-log.md)。

## 2026-09-09：头文件接口注释统一

- 修改全部 14 个项目头文件，按用户格式说明真实接口、协议、时序与复位/初始化；规范与取舍见 [coding-style.md](coding-style.md) 和 [AI 日志](ai-log.md)。
- 基线 d50bf5e；本轮头文件仅注释变化，未修改功能代码。去注释对比通过，全部头文件具有五项说明；差异空白检查通过。
- 实际执行 `cmake --build build --parallel 4`、`ctest --test-dir build --output-on-failure`，Release 构建退出 0，CTest 12/12 通过（6.20 秒）；后续仅对齐 ASCII 图框。未重复 Debug 测试。
- 本条与头文件修改同提交；保留用户已有 README 修改，未推进作业 4。

## 2026-09-09：核查 Instrumentation 开关等价证据

- 用户询问当前开关结果是否一致；核查现有实现与测试，结论见 [统一统计文档](archive/task-statistics.md#2026-09-09instrumentation-开关的实际验证范围)。
- 已有开关只控制事件 CSV，统计始终执行；作业 1 basic 有逐字节结果/统计对比且上一轮回归通过，作业 2、3 缺同类成对断言。未改代码或新增测试结果。

## 2026-09-09：Model 职责分层与 Trace 等价回归

- 用户提出三层结构；已抽取共享数值/延迟实现和 Instrumentation，保留原 SystemC 模块协议、时序与阶段范围。阅读入口与取舍见 [Model 分层](archive/model-structure.md)，协作与讲解见 [AI 日志](ai-log.md)、[学习日志](learning-log.md)。
- 重构前基线 0df1381 的 Release 12/12；重构后 Release/Debug 各 12/12，分别含 96 个 Trace 开/关成对场景；372 份基线文件哈希相同。主机 Profiling 3 次烟雾运行结果/统计一致。
- 原始构建错误、修复与验证命令见 [证据](evidence/model-structure-validation.md)；最终只调整注释与文档。全量 Instrumentation 总开关仍未实施。
- 源码、测试和记录一起本地提交；保留用户已有 README 修改，未推进作业 4。

## 2026-09-09：Transform 从输入到输出正向计算

- 根据用户可读性反馈，修改作业 1 Transform 实现与接口说明；采用下一状态统一提交，保留原时序、容量和同拍补位。决策见 [D-018](archive/decisions.md#d-018作业-1-transform-正向计算下一拍状态)，讲解见 [学习日志](learning-log.md)，协作见 [AI 日志](ai-log.md)。
- Release/Debug 构建退出 0，阶段 1 回归各 1/1（每种 24 场景/1434 任务）；102 份基线结果、事件与统计文件哈希全部一致。clang-format 检查 cpp 与 Git 差异空白检查通过；[详细证据](evidence/stage1-forward-validation.md)。
- 未推进其他阶段；用户已有 README 暂存修改不纳入本轮提交。用户理解审阅仍待完成。

## 2026-09-09：澄清倒序与背压的关系

- 用户提出正向数据流、倒序变量开销及背压的理解；补充隐式空槽传播、同拍补位和缓冲吸收背压的解释，见 [学习日志](learning-log.md)。
- 仅学习记录，无代码变化；核对现有 Transform 实现及前轮等价结论，未重复运行测试。局部变量减少不作为主机性能结论。

## 2026-09-09：按用户选择恢复倒序

- 作业 1 Transform 恢复下游优先原地更新，源码/头文件补充空槽反向传播、同拍补位和防跨级说明；[D-019](archive/decisions.md#d-019用户选择恢复作业-1-倒序原地更新) 替代 D-018，历史保留。
- Release/Debug 阶段 1 构建和回归均通过，102 份基线文件哈希一致；cpp 格式与差异空白检查通过。[验证证据](evidence/stage1-forward-validation.md#2026-09-09用户选择恢复倒序)，讨论与用户判断见学习/AI 日志。
- 未改其他阶段，未纳入用户已有 README 暂存修改。

## 2026-09-09：详细解释三层架构

- 对照当前 Compute、planGcd 和 EventRecorder 解释数值、周期、事件、汇总、主机耗时与必要契约；补充 [分层阅读示例](archive/model-structure.md) 和 [学习记录](learning-log.md)。
- 仅文档更新，核对源码及既有单任务时间线，未重新运行模型测试。用户已有暂存 README 修改保持原状，不纳入文档提交。

## 2026-09-09：作业 4 独立初始版本

- 用户明确要把所需源码整合进 stage4 后再改模型；建立 30 个独立源码/头文件及 stage4_gcd，旧作业与旧共享实现保持原样。来源和取舍见 [初始版本](archive/stage4-initial.md)。
- Release/Debug 全量测试各 13/13，新旧模型各配置 276 份生成文件逐字节一致；[验证证据](evidence/stage4-initial/validation.md)。仍为逐周期模型，事件改造、性能比较与缺陷案例待完成。
- 代码与记录本地提交；用户已有 README 暂存修改不纳入。未推送远端。

## 2026-09-10：更新项目 README 定位

- 按用户要求，将 [README](../README.md) 改为 SystemC 架构与仿真实践入口，介绍持续学习、设计演进、优化验证与问题复盘；协作说明见 [AI 日志](ai-log.md)。
- 精简为探索方向、快速开始、文档导航与依赖，修正第一天进度的过时表述；保留历史记录，不更改阶段实现或验收状态。
- 验证：核对构建脚本、依赖版本和 README 本地链接，执行本轮文档差异空白检查；纯文档修改，未运行模型测试。
- 本轮仅提交 README 与两份日志；工作区已有阶段 4 源码、构建配置、测试和证据修改不纳入。未推送远端。

## 2026-09-10：Parser 定时输入与空间事件

- 按用户纠正保持原输入节拍；移除 Parser 时钟端口与 System 绑定，添加定时输入、满 FIFO 事件等待和 EOF 退出。其余模块保留时钟，详见 [D-021 与设计](archive/stage4-parser.md)。
- Release/Debug 构建通过、全量各 14/14；最终补强事件比较时间单调性后阶段 4 各 2/2。新增无时钟测试验证 1 ns 首次输入、5.5 ns 释放后 6 ns 恢复、无满队列/EOF 轮询。保留 [B-009 原始失败及回归证据](evidence/stage4-parser/validation.md)。
- 未改作业 1～3、未宣称完整事件模型或性能验收完成。代码与记录本地提交，不推送远端。工作期间另出现 README 提交 f158491，本轮未修改 README。

## 2026-09-10：全部移除时钟并对照作业 3

- stage4 全系统改为集中式最早事件调度，移除时钟对象、端口、边沿敏感和固定周期观察；绝对计算完成时间与区间统计保持原语义。[D-022 与设计](archive/stage4-events.md)。
- Release/Debug 全量各 15/15；31 个既有场景加 4 个边界场景，新旧结果、完成周期及统计一致。31 场景的事件 CSV 实际逐字节一致；百万周期慢输出为 8 次事件批次，最大跳跃 999,988 周期。
- 首次诊断日志比较失败已保留为 B-010，并修正适配；[逐场景对照和原始证据](evidence/stage4-events/validation.md)。无处理器功能/时序失败，不编造阶段 4 缺陷案例；正式主机性能比较待完成。
- 本轮源码、测试、记录一起本地提交，不修改旧阶段、不推送远端。

## 2026-09-10：完成时钟/事件模型性能与开销对照

- 增加可选 Windows profiling 构建目标、交替重复测量脚本与静态图；两模型生产源码不变。10 配置、140 次正式测量，独立 GCD 与输出/统计哈希一致；有 Trace 两组日志也一致。Release 原有 CTest 15/15。
- 报告覆盖功能、系统周期/吞吐/延迟/利用率、模型/进程耗时、CPU、峰值内存、delta 轮次、跳过比例及同输入 Trace 成本。[完整报告及图](archive/model-comparison.md)，原始样本见其证据链接。
- 长计算约快 5.78×，混合约快 2.85×；连续零延迟近似持平，Trace 可使事件模型更慢并增大内存。真实性能设计案例 B-011 已记录，优化尚未实施。
- 测量方法见 D-023；代码、图表与过程记录一起本地提交，不推送远端。

## 2026-09-10：统一运行时检查命名

- 按用户反馈，将公共运行时契约函数及所有生产/测试调用从 `requireCondition` 改为 `assertCondition`；当前规范同步更新，历史记录保留旧名并由 [D-024](archive/decisions.md#d-024运行时契约检查采用-assertcondition-命名) 说明替代关系。
- 验证：Release 完整构建成功；先运行 `ctest --test-dir build -R '^contract_' --output-on-failure`，契约检查 6/6 通过，再运行全量 `ctest --test-dir build --output-on-failure`，15/15 通过。语义仍为条件失败抛所选异常，Debug/Release 均执行。

## 2026-09-10：解释任务统计的 `BOUNDARY_COUNT`

- 用户澄清提问对象为 `TaskStatistics::BOUNDARY_COUNT`；核对 stage4 的 task_statistics 头/实现，说明五个事件边界如何驱动时间线、样本数组、完成判定和四段加端到端统计。
- 仅新增学习与协作记录，未改模型或运行测试；详细理解沉淀见 [L-016](learning-log.md#l-016boundary_count-是统计边界数量不是普通局部-const)，协作更正见 [AI 日志](ai-log.md)。
- 后续按用户要求读取全部 `constexpr` 声明，准备按用途列举；仍仅为源码阅读，不修改模型或运行测试。


## 2026-09-10：修复用户审查的四项问题

- 核实并修复五个模型入口的 ns 单位换算与超长参数诊断，清除 Stage4 死统计字段并显式整理头文件依赖；[逐项判断和实现](archive/review-fixes.md)。
- B-012/B-013 修复前新增测试 10/10 失败，原始证据保留；修复后 Release/Debug 各 25/25。八个相关头文件单独包含编译及五个实际 CLI 超长参数检查通过。
- 默认周期/功能/时序不变，未重测主机性能。代码、证据和本轮记录本地提交；已有其他未提交学习记录保持原状，不推送远端。

## 2026-09-10：设计作业 1～4 的统计与报告方式

- 新增 [指标与呈现方案](archive/metrics-presentation.md)，给出统一计时边界、公式和覆盖状态、阶段表格/图形、单因素实验与最小场景矩阵；关联 D-025。
- 已核对各阶段现有统计实现及任务统计、保序影响、双模型测量文档；已有数据只作为历史依据，未生成新实测结果。轮前关于 constexpr 的学习记录保持原状，单独留在工作区。
- 本轮为文档设计，核对差异和本地链接，不运行无关模型测试；模型实现与阶段验收状态不变。后续可先从已有 CSV 派生报表，再按需要补计数器。

## 2026-09-10：第二轮规范与风格修复（Stage4）

- 按用户要求实施审查意见 5～12：容量类型统一、重复信号读取、注释错位、EventRecorder const/mutable、Usage 字段、用法字符串常量化、内部不变量 logic_error、限定符一致；[逐项记录](archive/review-fixes.md)。
- WSL g++ 11.4 环境验证：Debug/Release 各 25/25 通过、-Wall -Wextra -Wpedantic 零警告；用法字符串与统计输出逐字节不变。同日补测 Windows（Strawberry MinGW）双配置重建零警告、ctest 各 25/25，exe 输出与 WSL 一致（仅 CRLF 文本模式差异）；[验证记录](archive/review-fixes.md)。
- 本轮无新缺陷案例，仅风格与维护性修改，不改任何数值。代码与记录本地提交；工作区另有用户未提交的 constexpr 学习记录，随日志文件一并保留。


## 2026-09-10：讲解 Stage4 的 SC_METHOD 与 advance

- 核查 System::runEvents、Dispatcher::route、Compute 完成期限及 Parser 节拍；解释信号变化、事务事件与时间推进的区别，见 [学习记录](learning-log.md#2026-09-10sc_method-与事件调度不是互斥选择)。
- 仅追加讲解记录；核对本地 SystemC 源码及 Accellera sc_wait.cpp，未修改模型或重跑测试。工作区其他改动保留，不纳入本轮提交。

## 2026-09-10：按指标设计实测作业 1～4 数据

- WSL 原生 Release 构建（build-linux/，CTest 25/25 通过）后运行 `scripts/run_metrics_matrix.py`：75 次模型运行＋160 次主机测量，覆盖设计文档的最小场景矩阵、容量/节拍/窗口/结果深度扫描与 Trace 开关对照；每次运行独立 `math.gcd` 校验，另通过 Trace 开/关统计一致性与重复运行确定性检查。
- 四阶段表图与结论见 [实测结果](metrics-results.md)，原始证据在 [evidence/metrics-matrix/](evidence/metrics-matrix/)（runs/resources/profiling_samples 等 CSV＋零依赖 SVG）。要点：`T=B+N+5` 契约验证；容量不改饱和吞吐、节拍 P>7 时吞吐=1/P；双实例计算受限加速比≈2.0、偏斜负载 1.06×、W≥块长后恢复 2.0×；两模型功能时序完全等价，事件模型慢输出/稀疏等待快 22×/30×（Linux 进程口径），B-011 在 Linux 复现。
- 设计中"新增"的连续阻塞段以逐周期 Trace 离线导出完成（与模型计数核对一致）；满载占比、窗口驻留分布仍未实施。Windows 入口径效率基线保留引用，未重测。本轮未修改模型源码；新增两个脚本、报告与证据本地提交。

## 2026-09-10：双编译环境验证与文档

- 在真实 Windows PowerShell 中完整执行 `scripts/build-test.ps1`（Release、Debug 两配置，各 25/25，零警告）；Linux/WSL 侧按 README 新命令用 `build-linux/` 与 `build-linux/debug` 验证（各 25/25，零警告）；[四份日志](evidence/dual-environment/)。
- README 快速开始改写为双环境：Windows（PowerShell/MinGW/Ninja，`build/`）与 Linux（bash/GCC/Make，`build-linux/`），含 SystemC 获取、配置/编译/测试命令与 gitignore 说明。
- 本轮不改模型源码；代码与证据随文档本地提交，不推送远端。

## 2026-09-10：新增 SystemC Web 查看器工具

- 用户提出用 Python 脚本 + 前端网页动画展示 SystemC 模块链接与运行周期；AI 给出设计并实现首版，关联 [D-026](archive/decisions.md#d-026)、[A-019](ai-log.md#2026-09-10设计并实现-systemc-web-查看器a-019)。
- 新增 `scripts/scviz.py` 与 `scripts/scviz/`（model/sc_parser/events/webgen/template.html），零第三方依赖；生成 `web/stage1.html`（自包含，内嵌结构 + 事件 trace）。
- 验证：对作业 1 源码运行后解析得到 top=System、模块 m_clk/m_parser/m_transform/m_compute/m_output（拓扑序）、7 根连线（4 时钟 + 3 FIFO）、8 条任务、end_cycle=79，与 stage1-stats.csv 一致；生成 HTML 经 Node 校验 JS 语法通过。未在浏览器中人工目测动画（沙箱无图形浏览器），视觉与交互效果待用户打开确认。
- 设计文档见 [web-viewer.md](archive/web-viewer.md)；生成物 `/web/` 加入 .gitignore，按命令重建。

## 2026-09-10：提交导出脚本与最终设计文档初稿

- 新增 `scripts/export_project.py`：一键把六类内容（SystemC 源码、编译配置、测试、功能输出、性能统计、设计文档）复制到根目录临时 `export/`（已加入 .gitignore，可随时重建）；自动生成 README 清单（生成时间、Git HEAD、目录说明、文件计数与复现命令）。实测导出 1128 个文件：68 个唯一运行配置的功能输出与统计、13 个场景输入、25 个汇总表/图。
- 按用户要求重写 [最终设计](final-design.md)：压缩为概述/时序契约/决策/验证/实测发现/边界/索引七节，纳入四阶段完成态与实测结论；替代 2026-09-07「作业4待实现」版本（历史在 Git）。该文档为 AI 草稿，待用户审阅修订。
- 修复导出脚本两处缺陷：profile 目录场景名提取用了原始目录名（误将模型名当场景）、内容去重把同名异内容写法颠倒；修复后按场景名各存一份并检测同名冲突。
- 按用户要求把导出目录改为与提交要求一一对应的编号结构：01_systemc_sources、02_build_scripts、03_test_inputs、04_functional_outputs、05_performance_stats、06_design_docs；README 清单同步标注对应关系。
- 用户指出编号目录会破坏构建脚本的相对路径假设，决定改回原始项目布局（CMakeLists.txt、src/、tests/、scripts/ 同级），六类内容与目录的映射改记在生成的 README.md 中，并加入一键构建/测试与结果复现命令（Windows/Linux 双平台）。
- 实际验证导出包可独立构建与复现：链接 third_party 后 `cmake -S export` 构建成功；README 中的 stage1/混合与 stage3_window/偏斜复现命令运行后，功能输出与统计 CSV 均与导出结果逐字节一致（统计不含主机耗时，同源码同输入确定性成立）。

## 2026-09-10：SystemC Web 查看器扩展到作业 1～4

- 应"生成 1~4 的网页看看效果"，把查看器推广到四个阶段：解析器新增 `sc_signal` 握手连线、`sc_vector` 向量端口/通道展开、计算扇出的循环/指针数组绑定展开、`ready`/`base` 背压反向边识别，事件载入兼容阶段 2～4 的新增结构事件（`link`/`compute_unit`/`reorder_*`/`window_store` 等）。修正三处自身缺陷：数组模板嵌套尖括号解析失败、带 `{"label"}` 的信号通道漏检、布局把前向边误判为反向边。
- 生成 `web/stage1..4.html` 及 `web/index.html`（`web/` 已 gitignore，按命令重建）。已核对：各页模块/连线/方向/背压边与源码一致（stage2 握手、stage3 扇出与重排、stage4 事件模型无共享时钟），JS 语法经 Node 校验通过；动画视觉仍待用户在浏览器确认。
- 文档更新 [web-viewer.md](archive/web-viewer.md)；设计取舍见 [D-026](archive/decisions.md#d-026)。


## 2026-09-11：解释作业2阻塞统计的含义

- 对照 Stage2 报表实现和历史 slow_sink CSV，将握手阻塞、结果等待、缓存积压与吞吐/延迟关联，见 [学习记录](learning-log.md#2026-09-11作业2如何体现阻塞的性能影响)。区分现象解释与单因素对照才能证明的性能损失。
- 本轮只读代码与已有证据、追加文档；未改模型、未重跑测试，其他工作区改动保持原状。


## 2026-09-11：FIFO 与握手的数据对照和选型

- 新增 [证据与选型说明](archive/fifo-handshake-evidence.md)，核对历史 Stage1/2 三组输入输出哈希、容量/输出节拍扫描与慢输出时间线；参考 AMD 官方文档解释弹性缓冲和异步 FIFO。
- 仅文档分析，未运行新模型测试；结论区分已有实测与未覆盖场景，其他工作区修改不纳入提交。

## 2026-09-11：按开源审美实施 A 类风格修改（全阶段）

- 实施审查 A 类 1～8 项：[[nodiscard]] 查询函数、operator== 自由函数+!=、static→匿名命名空间、write*Stats 断言去重、消息改写、事件名 string_view、include 字母序、using 声明标注；[逐项记录](archive/review-fixes.md)。
- [[nodiscard]] 抓到 model_entry.cpp 丢弃返回值；verify.cpp nullptr 用例随契约更新，首轮构建即段错误暴露后修复。
- 四环境验证（Linux/Windows PowerShell 各 Debug+Release）均 25/25、零警告；正常输出逐字节不变，仅非法参数消息文本变化。代码与证据本地提交。

## 2026-09-11：按三方面要求精简融合文档

- 用户要求精简过多设计文档，协作记录按题目三方面组织。实施：31 个历史文档 git mv 到 docs/archive/（原文保留，附 README 说明去向），顶层仅留 8 个（ai-log、coding-style、final-design、learning-log、metrics-results、process、status、work-log）。
- 重写 [ai-log.md](ai-log.md)：按「向 agent 提出的重要设计要求（18 条时间线）／对 agent 方案的主要修改（9 例）／发现 agent 实现问题时的判断与处理（原则＋B-006/B-011 主动案例＋三轮审查＋AI 自身错误表）」组织，融合原 decisions/defects/review-fixes/engineering-audit 的协作内容；process.md 同步新分工，status.md 重写为完成态。
- 全库链接核查修复：保留文档指向归档文档的链接加 archive/ 前缀（learning-log 15 处、work-log/metrics-results/final-design/coding-style 若干）；归档文档指向顶层与 evidence 的相对路径上调一级；发现并修复 3 个 evidence 文件回链已归档文档的断链（此前未被检查覆盖）。
- 导出脚本同步：docs 顶层＋archive 整体导出，docs/evidence 全量复制使导出包内全部文档链接有效（暴露并修复了此前导出包 evidence 断链的历史缺口）；补充 profile_model.py；README 映射计数改为动态。仓库与导出包链接检查均 ALL OK。
- 融合取舍为 AI 方案：三方面的条目选取与摘录粒度待用户审阅；原文均在 archive/ 与 Git 历史，未静默删改。

## 2026-09-11：修复阻塞率图中重合序列的显示问题

- 用户发现 a2_depth_blocked 图中 S/V 序列疑似为 0。核实数据无误（S/V 全深度恒 0.979、S/T 0.9789，两者仅差 0.01 个百分点），根因是两条折线数值几乎相同、像素级完全重叠，后画的 S/T 实线盖住了 S/V。
- 修复：metrics_svg.line_chart 增加 dashed 参数，S/T 改虚线并在标题注明"与 S/V 几乎重合"；a2_depth/a2_period 两张阻塞率图从既有 CSV 重新生成（未重跑测量），run_metrics_matrix.py 同步该样式，导出包已刷新。

## 2026-09-11：为理解性添加注释（骨架标注 + 调度语义）

- 为帮助通读 stage1～4：五个 main.cpp 文件头标注 CLI 骨架函数（parsePositiveInteger/parseOptions/checkDistinctPaths/flushFiles/openTestEvents）逐字共享、只需精读 stage1 一份；stage4 runEvents 补齐三个零时间 delta 的逐步语义（提交→组合求值→输出生效）；stage3 Dispatcher::tick 补轮转前进语义及与 stage3_window 择闲分水岭的说明。
- 纯注释变更，零行为改动；相关测试（stage3 轮转 + stage4 全套）通过、零警告。此前分析中的可选项 3/4（collector 小重构）、6（parseTaskLine 提取）与模块×阶段矩阵待用户决定。

## 2026-09-11：折线图纵轴改为数据自适应范围

- 用户指出阻塞率图纵轴 0~1 不专业、两个接近的值应有区分。实施：line_chart 线性轴自动适配数据跨度并吸附到 1/2/5 整数刻度（_snap_axis/_nice_step），刻度与数据标签精度跟随步长（_tick_label），支持 y_range 覆盖；柱状图保持 0 基线（长度编码的惯例），对数轴逻辑不变。
- 效果：a2_depth_blocked 轴变为 [0.97885, 0.97905]、刻度 5 位小数，S/V 与 S/T 两线分离约 155px，标签 0.97899/0.97889 可区分；恒定值序列（吞吐 0.021）自动展开为局部范围显示平坦线。修复过程发现非负数据误出 -1,000 刻度（钳制基准误用填充后下界），改为按数据最小值钳制。
- 6 张折线图与 3 张柱状图全部从既有 CSV 重新生成（未重跑测量），SVG 校验无异常；导出包已同步。

## 2026-09-11：解释 a3_cycles 中轮转与窗口版差距小的原因

- 用户询问为何该图中"使用缓存窗口"与"未使用"差距不大。基于 runs.csv 核对：short/long 同质任务完成有序（保序等待 0，T 仅差 1 拍流水常数）；mixed 计算受限（利用率≈98%），保序等待与计算忙重叠不延长 T，窗口仅赚 1415 拍（择闲喂引擎略满）；skew 有 4.2% 收益但被 W=8 信用上限压制（window_blocked=34500 全部算力可用，理想 T≈18.5k，W=32 后达 23777）。
- 另指出 stage3 的 R=2 结果 FIFO 本身已是 4 项隐式乱序缓冲，"有无窗口"并非"有无缓存"。已在 metrics-results.md §3.1 补读图提示，导出包同步。

## 2026-09-11：整理作业 3/4 主动案例的写作口径

- 核对历史 B-006/B-011 与当前 Trace 实现，按题目四项要求提供报告措辞；区分旧轮转版容量改进与当前窗口版，明确 B-011 优化尚未实施、修正闭环待完成。
- 本轮仅整理说明，未修改代码或重跑实验；记录见 [AI 协作](ai-log.md) 与 [学习记录](learning-log.md)。

## 2026-09-11：澄清 B-011 属于 Trace 观测实现问题

- 根据用户关于“采样实际是打点”的判断，核对 EventRecorder 与 Usage，区分事件日志、逐周期诊断展开和占用加权累计；明确 B-011 的子系统归属及作为核心模型缺陷案例的范围限制。
- 本轮只更新解释，未改代码、未重测；详见 [AI 记录](ai-log.md) 和 [学习记录](learning-log.md)。

## 2026-09-11：perf 驱动的 Stage4 Trace 优化

- 按用户要求从主机采样定位 B-011：保留 b2207d3 旧二进制，perf 前/后 3391/2973 样本；针对全量排序和流格式化热点实施按区间/批次合并与缓冲编码，保持完整 Trace。
- 新增独立 Trace 顺序/数值/生命周期/写入错误回归与可复用采样/测量脚本；84 次无 profiler 干扰的成对实验，混合 Trace 302.77→83.83 ms（3.61×），峰值 RSS 73980→21052 KiB，全部输出/统计/Trace 哈希一致。
- B-011 从“建议未实施”更新为实际改进闭环。详细复现、热点、实验环境、原始失败与回归见 [证据报告](evidence/perf-trace/README.md)；同步 [状态](status.md)、[设计](final-design.md)、[实测](metrics-results.md)、[AI 记录](ai-log.md) 与 [学习记录](learning-log.md)。未推送远端。

## 2026-09-11：重新导出提交包（含进行中修改）

- 在含未提交修改（event_recorder Trace 优化延续、fair-trace 证据、Windows 日志刷新）的工作区重跑 export_project.py：2082 个文件，零缺失零告警。
- 验证：工作区 26/26 → 导出包挂 third_party 独立构建零警告、ctest 26/26；README 两条复现命令（stage1 混合、stage3_window 偏斜）输出与统计均逐字节一致；链接检查仅剩 1 处真实断链：perf-trace README 指向尚未编写的 fair-trace/README.md，待该轮工作收尾后补写并重导。
- 导出后已清理验证用的 build/ 与 third_party 链接；本轮仅追加日志，未提交（避免与进行中的 fair-trace 修改混合）。

## 2026-09-11：开源化收尾（LICENSE/CI/标签/.vscode 说明）

- 按开源审美审查结论实施用户确认的 5 项：新增 MIT LICENSE（AI 默认选择待用户确认，换协议只需换此文件）；新增 GitHub Actions CI（Linux Release 构建+全量 ctest，SystemC 3.0.1 固定 commit 缓存，YAML 本地解析验证，命令与已验证的 Linux 路径一致）；在四个作业状态翻转为完成的提交上打 annotated 标签 stage1～stage4；.vscode/settings.json 注明换机只改 scGcd.toolchainBin 一处；删除根目录遗留的 day01_basics.vcd。
- README 加 CI 徽章（私有仓库期间外部不可见）、MIT 声明与里程碑标签说明。文件头 All rights reserved 与 MIT 并存的表述张力未批量改写（340+ 文件），LICENSE 文件为许可依据。
- 决策：保留 .vscode 提交（文档链 + 机器路径已集中化）；stage3_window 命名与 src 阶段复制保留为有意取舍。提交并推送 main 与全部标签。
