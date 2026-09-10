# 用户代码审查四项修复

2026-09-10；用户提出四项具体问题，AI 对照基线 `9e613e9` 复核并修复。

| 项目 | 判断 | 修改范围 |
|---|---|---|
| simulated_time_ns 直接输出周期数 | 潜在单位错误；默认 1 ns 下数值恰好正确 | stage1、stage2、stage3、stage3_window、stage4 的 main 均显式用周期数乘本阶段周期时长 |
| m_idleOtherBlockedCycles | Stage4 未使用的复制遗留字段 | 仅删除 Stage4 字段；旧轮转 Stage3 仍使用，保留其共享字段 |
| stoull 超范围异常 | 非法输入能被拒绝，但诊断与正常超限不一致 | 五个入口改为十进制逐位解析，在乘加前检查 limit，统一抛 runtime_error("option out of range") |
| payload.hpp 包含 Parser/Output | 无必要耦合和隐式依赖；原先并非完全不能独立包含 | Payload 仅保留自身需要的标准库/SystemC；Compute、Transform、Dispatcher、Collector 直接包含 types.hpp，main 直接包含 Parser/Output |

保留模拟周期、字段名、默认 1 ns 配置、正常参数范围、前导零规则及模型行为。时间指标仍表示已观察周期对应的 ns，不改为 cycles，也不把测试用非 1 ns 参数宣称为整个工程已支持任意周期配置。

## 先复现再修复

新增 `tests/contracts/model_entry.cpp`，各入口单独构建。测试直接调用真实入口中的 writeStats 与 parsePositiveInteger：报表测试在该测试翻译单元提供 2.5 ns 单位、设置 7 个已观察周期，应输出 cycles=7、simulated_time_ns=17.5；不运行仿真，也不修改生产默认单位。

修复前五个报表测试均失败，五个参数测试均报 `FAIL: stoull`；10/10 失败，退出 8。[原始失败](evidence/review-fixes/before-tests.log)。对应 B-012/B-013；这是用户触发的检查，不记为用户亲自改代码或 AI 独立发现。

修复后参数测试覆盖 30 位 9、超过业务上限、0、恰等于上限、很长的前导零和 uint64_t 最大值；并额外在五个实际 CLI 中确认超长值退出 1、输出 `FAIL: option out of range`。

## 最终验证

- `cmake --build build --parallel 4`、`cmake --build build/debug --parallel 4` 均退出 0。
- `ctest --test-dir build --output-on-failure`：25/25，[Release 日志](evidence/review-fixes/release-tests.log)。
- `ctest --test-dir build/debug --output-on-failure`：25/25，[Debug 日志](evidence/review-fixes/debug-tests.log)。
- Stage4 的 payload/types/parser/output/compute/transform/dispatcher/collector 头文件逐个用独立翻译单元进行 C++17 语法检查通过；实际 CLI 诊断也通过。[记录](evidence/review-fixes/headers-and-cli.txt)。
- 新旧模型等价、原有功能/时序/背压测试继续通过；默认 1 ns 结果和统计未改变。格式和 Git 空白检查通过。

本轮未重测主机性能，不改历史性能报告。开始时 docs/ai-log.md、docs/learning-log.md、docs/work-log.md 已有其他未提交学习记录；保留这些修改，本次提交仅纳入本轮追加的 AI/工作记录。

## 同日第二轮：审查意见 5～12（规范与风格）

用户确认 1～4 修复完成后要求继续处理第二轮八项；AI 在 `9b34b47` 基线上实施，范围仅限 Stage4。

| 项目 | 判断 | 修改 |
|---|---|---|
| Options 容量字段 int/unsigned 混用 | 容量语义应统一 | m_depth/m_resultDepth 及对应默认/上限常量统一为 unsigned；fifoCapacitiesTasks 数组同步 |
| transform.cpp 重复读取 m_readyIn | 同一 delta 内两次 read() 造成两次采样语义误导 | 读一次存入局部 `ready` 后复用 |
| compute.hpp deliver() 前孤立注释 | advance() 的契约注释错位悬挂 | 移至 advance() 声明处；deliver() 保留自身说明 |
| EventRecorder const + mutable | const 方法经 mutable 改状态，语义异味 | 四个方法去掉 const，成员去掉 mutable；头文件 Protocol 注明非 const 设计意图 |
| Usage 字段顺序与类型 | m_last 与其他字段分隔且类型不一 | 三字段统一 uint64_t 并集中声明；sample 参数改 uint64_t，sampleUsage 调用侧同步 |
| sc_main 用法字符串硬编码默认值 | 与常量双份维护易失同步 | 改为 ostringstream 从常量生成，输出文本不变 |
| 内部不变量用默认 runtime_error | 违反自身 logic_error 约定 | runEvents 死锁检查及 transform/dispatcher/compute/collector 的 accountSkipped 检查改 logic_error |
| stage4:: 限定不一致 | 同命名空间内混用 | dispatcher/collector 头/源去除冗余限定；main.cpp 删除 using namespace 下多余的 using 声明 |

行为保持：用法字符串、统计数值、模拟周期与输出逐字节不变（第 10 项仅改文本生成方式，内容相同）。本轮无运行时缺陷，未新增缺陷案例；其中“compute.hpp 文件级两个 @brief 重复”经复核为 @file 与 @class 各自摘要的 Doxygen 惯例，不属重复，未改。

验证在 WSL g++ 11.4 环境完成：Debug/Release 各 25/25（[Debug](evidence/review-fixes/style-round-debug-tests.log)、[Release](evidence/review-fixes/style-round-release-tests.log)），-Wall -Wextra -Wpedantic 零警告。同日补测 Windows 环境（Strawberry MinGW g++ + Ninja，经 WSL interop 调用）：`build`（Release）与 `build\debug` 均 87/87 目标零警告重建，ctest 各 25/25（[Windows Release](evidence/review-fixes/style-round-windows-release-tests.log)、[Windows Debug](evidence/review-fixes/style-round-windows-debug-tests.log)）；stage4_gcd.exe 用法字符串与 WSL 一致、非法参数退出码 2，同输入模拟输出与统计与 WSL 逐字节一致（仅文本模式 CRLF 差异）。提交时工作区另有用户未提交的 constexpr 学习记录，随日志文件一并保留。

## 同日第三轮：开源审美扫描的 A 类实施（1～8 项）

用户要求按 GitHub 优秀开源 C++ 项目审美再审；AI 区分 A 类（与自身规范兼容）与 B 类（与文档化决策相悖，未改），用户确认实施 A 类 1～8 项，覆盖全部阶段与共享层。

| 项目 | 修改 |
|---|---|
| 查询函数补 `[[nodiscard]]` | 各阶段 canRead/idle/empty/occupancy/nextDelay/hasCredit/selectedUnit/drained 及 parsePositiveInteger、nextRandom、bits |
| `operator==` 改自由函数并补 `!=` | stage2/stage4 payload，定义移入 payload.cpp，两侧对称 |
| 文件局部函数 `static` → 匿名命名空间 | 两份 gcd_plan.cpp 的 bits、两份 dispatcher.cpp 的 nextRandom，与 main.cpp 既有写法一致 |
| write*Stats 前置断言去重 | 入口 writeStats 保留检查，五个阶段 14 处辅助函数重复断言删除（均仅由 writeStats 调用） |
| 错误消息改写 | "expected parsePositiveInteger integer option" → "expected a decimal integer option"（五入口）；去重后 "statistics stream is not writable" 仅余入口与 TaskStatistics 各一处，不再提取常量 |
| 事件名参数统一 `std::string_view` | 两份 EventRecorder 的 record/repeat/append；非空断言随参数类型移除，头文件契约同步 |
| include 组内字母序 | 五个 main.cpp 及 gcd_plan/transform/event_recorder 等自身头文件组按完整路径字符串重排 |
| 头文件 using 声明标注知情例外 | stage1/stage4 types.hpp 的 model:: 再导出注释说明 |

[[nodiscard]] 立即抓到真实案例：model_entry.cpp 测试故意丢弃 parsePositiveInteger 返回值，MinGW GCC 13 报 -Wunused-result，改用 static_cast<void> 显式声明意图。verify.cpp 的 event 契约用例随参数类型更新：原 nullptr 非空检查已随 string_view 契约移除（构造自 nullptr 为 UB，首次构建即段错误暴露），改为测试仍存在的"事件流不可写抛 runtime_error"契约。

行为影响仅限非法参数消息文本；正常路径输出与统计逐字节不变（人工比对 + CLI 冒烟）。验证：Linux g++ 11.4 与 Windows PowerShell（MinGW GCC 13.2）各 Debug/Release 25/25、零警告（[Linux Release](evidence/review-fixes/style-a-round-linux-release-tests.log)、[Linux Debug](evidence/review-fixes/style-a-round-linux-debug-tests.log)、[Windows Release](evidence/review-fixes/style-a-round-windows-release-tests.log)、[Windows Debug](evidence/review-fixes/style-a-round-windows-debug-tests.log)）。
