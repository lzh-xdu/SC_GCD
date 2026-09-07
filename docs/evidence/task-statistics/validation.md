# 统计补充验证

日期：2026-09-07；基线 438615f 加本条所在提交的 C++/测试修改。仅补观察，不改变规定延迟、状态转换、容量、随机策略。

- Debug：`./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin`，6/6 通过。
- Release：同脚本 `-SkipTests` 构建默认 build，随后 `ctest --test-dir build --output-on-failure`，6/6 通过；本轮未重现旧 summary.csv 权限失败。
- 96 个有效场景：阶段 1/2/3轮转/3窗口各 24/19/22/31；额外保留各阶段非法输入、超时、关闭事件输出测试。全部新统计在开启/关闭事件 CSV 时保持一致。
- 新增独立校验由事件 CSV 重算每条任务四段延迟、总和、均值、最大值、P95；从每个 Compute 的接收至结果交付占用沿集合反算空闲未接收周期；从派发沿之前的退休记录和单元占用反算“无信用但有 ready”。
- 96 场景与修改前 Release 输出对照：所有旧指标值相同，功能文件和事件 CSV 字节相同，见 behavior-comparison.json。完成对照后才运行新版 Release 测试覆盖原输出目录。
- 本轮 Debug/Release 四份 summary.csv 字节一致。原始测试报告见 debug-tests.log、release-tests.log；四阶段摘要与代表性输入/输出/事件/stats 均在本目录。

## 实测例子

`(48,18)`：阶段 1 的任务延迟 11=4+6+0+1，总执行周期 12；阶段 2 的任务延迟 10=3+6+0+1，总执行周期 11。差异来自移除中间 FIFO；任务从第 1 沿才入模，因此任务延迟比总周期少 1。

窗口版 slow_sink：40 项 `(30,30)`，D=R=1、W=8、seed=1、输出周期13；总周期520，平均任务延迟128.625=27.75+1+0+99.875，P95=153。窗口阻塞352，其中有 ready 的352；输出 FIFO 满导致 Collector 阻塞461。不同模块等待重叠，不能相加成系统损失。

局限：未测主机统计开销或真实 PPA；没有更改架构来验证某阻塞的可消除周期。首次一次性比较脚本误包含非法输入目录，因该目录无 events.csv 而失败；筛选双方存在事件文件的有效场景后完成以上96组比较，未改产品代码规避问题。
