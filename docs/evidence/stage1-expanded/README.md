# 扩展测试证据

日期 2026-09-06。代码基线 3db794c，加本目录所在提交的 main.cpp 诊断通道调整、新输入及 verify.py 扩展；不宣称基线已含本轮修改。

- initial-test-failure.log：原始首轮 CTest 失败，测试断言写死峰值 1，与实际容量 4 冲突，见 B-005。
- debug-tests.log / release-tests.log：修正后实际 CTest 原始日志，各 2/2 通过；本机 CTest 时间戳使用系统本地编码，原始文件保留。
- summary.csv：24 个有效场景的 Debug 实际统计汇总；已与 Release 汇总逐字节比较一致。
- 各场景子目录 output.txt / stats.csv：从实际 Debug 运行复制，独立保存功能与性能结果。固定输入/预期见 tests/stage1/cases；既有场景输入由 verify.py 确定生成。
- invalid_*/：实际非法输入及 run.log，非零退出已由回归验证。

完整逐任务事件留在 build/debug/stage1-tests，按 [测试矩阵](../../stage1-test-matrix.md) 可重建。没有为本轮性能比较启用额外的主机耗时结论；CTest 秒数只是测试运行耗时，不是作业 3/4 模型效率证据。
