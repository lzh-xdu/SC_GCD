# Parser 事件迁移验证

2026-09-10；旧源码为 `1f252ab`，新源码及测试为本条所在提交。工作期间另出现 README 文档提交 `f158491`；未修改或纳入本轮 README 内容。

命令与结果：

| 命令 | 结果 |
|---|---|
| `cmake --build build --parallel 4` | Release 构建退出 0 |
| `cmake --build build/debug --parallel 4` | Debug 构建退出 0 |
| `ctest --test-dir build --output-on-failure` | 14/14，退出 0 |
| `ctest --test-dir build/debug --output-on-failure` | 14/14，退出 0 |
| `ctest --test-dir build -R stage4 --output-on-failure` | 强化事件时间顺序断言后 2/2，退出 0 |
| `ctest --test-dir build/debug -R stage4 --output-on-failure` | 同上 2/2，退出 0 |

原始日志：[Release](release-tests.log)、[Debug](debug-tests.log)、[最终 Release](release-focused.log)、[最终 Debug](debug-focused.log)、[详细通过摘要](checks.txt)。范围及限制见 [Parser 设计](../../stage4-parser.md)。

首次 `ctest --test-dir build -R stage4_initial_equivalence --output-on-failure` 退出 8，报告 basic/events.csv 不同：[失败日志](initial-test-failure.log)。保留 [旧事件](initial-baseline-events.csv) 与 [新事件](initial-candidate-events.csv)。当时 29 个场景的事件 CSV 行序变化，其余结果与统计无差异；basic 中 id=1 的 parser_send 和 id=0 的 transform_accept 都是 cycle=2，顺序交换，事件内容未变。

修正比较方式：分别严格比较 parser_send 流与剩余事件流（含所有字段、时间和重复次数），并要求完整事件流时间非递减；因此仅允许同一时刻的跨进程交错变化，不忽略事件缺失、提前/延后或流内顺序变化。既有独立数值、握手、容量、统计断言继续运行。
