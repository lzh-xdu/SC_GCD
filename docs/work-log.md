# 工作与 AI 协作记录

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
- 修改：学习记录 [L-004](learning-log.md#l-004从-c-到-systemc再到电路)、AI 协作记录 [A-006](ai-log.md#a-006systemc-全景教学与硬件生成边界)。
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
- 修改：CMake 开启 `CMAKE_EXPORT_COMPILE_COMMANDS`；新增 `.vscode/settings.json` 和 `.vscode/extensions.json`，让 C/C++ 扩展复用真实 GCC/CMake 编译参数。协作详情见 [A-007](ai-log.md#a-007配置-vs-code-的-c-语法解析)。
- 验证：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Strawberry/c/bin/g++.exe`、`cmake --build build --parallel 4` 和 `ctest --test-dir build --output-on-failure` 均退出 0，CTest 2/2 通过；编译数据库确认 Stage 1 含 SystemC include、`SC_ENABLE_ASSERTIONS` 和 C++17。
- 范围：没有改变功能及时序；未包含用户已有的 `README.md` 修改。

## 2026-09-06：Stage 1 trace 终端可视化

- 用户请求以 Python 终端交互图形理解过程和效率；新增 scripts/trace_tui.py、针对性单元测试和[中文使用说明](trace-viewer.md)，在 stage1-run.md 增加入口。
- 实际输入得到 8 任务、79 事件周期、66 忙周期、83.54% 利用率、结果阻塞 0；当前数据主要体现计算与上游积压，不能度量主机模拟速度。
- 5/5 工具测试通过；实际 Windows 伪终端导航、播放与退出通过；[验证及快照](evidence/stage1/trace-viewer-validation.md)。不修改 C++，不重复模型 CTest，不推进后续阶段。
- 协作记录 [A-008](ai-log.md#a-008python-终端-trace-可视化)，学习记录 [L-006](learning-log.md#l-006用-trace-区分驻留计算与主机运行效率)。下一步由用户结合时间线审阅现有时序，尚未实施优化设计。

## 2026-09-06：解释 Day01 VCD 与查看方式

- 实读 build/debug/day01_basics.vcd、示例源码及本地 VCD tracing 源码；核对 GTKWave 官方文档，补充 [波形说明](inspect-simulation.md) 和 [L-007](learning-log.md#l-007vcd-与实际波形末尾的区别)。
- 新观察：文件末尾 #25 无信号变化；保留事实，未将代码预期冒充波形记录，未实施末沿记录修复。
- 文档轮次，未安装查看器、未运行模型测试；本机 PATH 无 gtkwave。未触碰 README 原有改动。
