# 优化审查配置证据

日期2026-09-07，基线338534d。模型未改动；新增探测脚本tests/stage3/optimization_probe.py。

执行：设置现有MinGW运行路径后，python tests/stage3/optimization_probe.py build build/optimization-review。
58组Release配置全部正常退出且由math.gcd独立检查输出；summary.csv记录模拟周期/忙周期/等待/载荷容量。不存在的指标留空，不当作0；R仅用于stage3，W仅用于window模型。

results下保留每组实际input/output/stats，manifest记录各exe摘要、代码基线、seed1和关闭事件记录。本次不测主机耗时，不修改阶段验收结果，不将配置扫描当成完整时序回归。

结果解释、适用范围、建议与优先级见../../stages1-3-optimization-review.md。
