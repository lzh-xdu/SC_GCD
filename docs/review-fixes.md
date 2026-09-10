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
