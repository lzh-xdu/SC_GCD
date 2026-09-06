# 风格与 Debug 验证

2026-09-06；基线 7558cfe 的模型加本提交重构，另保留 719360f 的 VS Code 配置。

- initial-test-failure.log：真实 Python 关键字重命名错误，见 B-004。
- release-tests.log：修复后 Release 2/2 通过。
- debug-tests.log：Debug 2/2 通过。
- GDB 命中/单步通过会话工具输出验证：Parser::tick m_sent=0、Testbench::run m_testFailures=0；均 next 后继续正常退出。未另存完整 GDB 输出，因此不伪称存在原始日志文件。
- 基本统计与 docs/evidence/stage1/basic.stats.csv 完全一致；测试数据及数值预期未改动。

复现方法见 ../../vscode-debug.md。
