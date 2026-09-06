# 新旧调度与窗口证据

日期2026-09-06～07，旧源码基线ad4b33e；新模型为本目录所在提交的新增stage3_window代码。

- baseline：新模型编写前实际运行的32组旧Release数据，保留input SHA256、exe SHA256、output、stats、run.log及summary；不是用新模型冒充旧方案。
- optimized：实际68组新Release数据，同16输入×4种子，加4个窗口扫描；输入由tests/stage3及stage1/cases固定保存。
- comparison.csv：按输入摘要、计算工作量和输出节拍核对后的汇总；包含等声明载荷容量的旧R=6对照。
- head-old、head-new：首项长任务的实际事件，首项完成前分别有1/7个后续任务完成；输出最终相同但总周期120/121。
- initial-test-failure.log：旧慢输出断言不适用于窗口credit方案的真实测试失败，见B-008。
- debug-tests.log、release-tests.log、release-stage1.log：最终相关回归结果。
- validation-summary.csv：新窗口验证场景统计；与Release相同。窗口1、2、8、慢输出和随机种子复现均通过。

复现方法和统计边界见../../stage3-window.md。原始日志可能含本机路径及本地时区编码，保持原始证据。没有伪称死锁已发生，只有设计风险与预防性满窗口测试。
