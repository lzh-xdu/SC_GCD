# 全系统事件迁移验证

2026-09-10；实现基线 `d78a477`，新源码和测试为本条所在提交。原作业 1～3 和共享源码无修改；比较目标是 `stage3_window_gcd`，不是旧轮转版 `stage3_gcd`。

| 命令 | 结果 |
|---|---|
| `cmake --build build --parallel 4` | Release 构建退出 0 |
| `cmake --build build/debug --parallel 4` | Debug 构建退出 0 |
| `ctest --test-dir build --output-on-failure` | 15/15，退出 0；[日志](release-tests.log) |
| `ctest --test-dir build/debug --output-on-failure` | 15/15，退出 0；[日志](debug-tests.log) |

`stage4_initial_equivalence` 保留历史名称，比较当前版本与作业 3；每配置 276 份产物，31 个有效场景和 9 类非法输入。正式输出、全部统计与事件 CSV 均一致；控制台只允许新增调度诊断。实际逐场景周期/批次记录：[Release](release-cycles.csv)、[Debug](debug-cycles.csv)。

`stage4_event_scheduler` 新增极慢输出、零延迟/最小容量、并行结果以及长队首四组配置，完整结果和统计逐字节对照。Release/Debug 均相同：[Release](release-scheduler.csv)、[Debug](debug-scheduler.csv)。极慢输出断言完成周期恰为 1,000,000、批次不超过 12、最大跳跃至少 999,000；实际为 8 批次、999,988 周期跳跃。

首轮阶段测试退出 8：[原始失败](initial-test-failure.log)。所有独立功能/时序断言已通过，`basic/run.log` 仅多出 `EVENT_SCHEDULER activations=33 max_jump_cycles=7`。修正测试只分离该行，仍比较其他诊断和全部结果/统计，不放宽模型数值或时间。首轮构建成功但测试目标有第三方 assert 内部变量警告，见 [首轮构建](first-build.log)；对测试目标也启用 SC_ENABLE_ASSERTIONS 后完成最终构建。

静态检查 `src/stage4` 无 sc_clock、m_clk、posedge、negedge 或旧 CLOCK_PERIOD_NS；时间换算文件改名，旧阶段 Git 差异为空。C++ 格式及 Git 空白检查通过。详细 Trace 的区间展开仅在记录层完成，跳过的时间不触发内核逐周期求值。

本轮未测量或宣称主机耗时加速，未把日志断言问题算作处理器缺陷案例。
