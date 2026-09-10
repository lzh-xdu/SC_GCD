# 作业 2：构建、观察与结果

接口及时序见 [设计](stage2-design.md)。保持作业 1 独立目标；src/stage2 下每个模块单独头文件/实现。

```powershell
./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin
./build/debug/stage2_gcd.exe tests/stage1/basic.txt build/debug/stage2-output.txt build/debug/stage2-stats.csv 2 build/debug/stage2-events.csv
```

参数与作业 1 一致：INPUT OUTPUT STATS [DEPTH=2] [EVENTS|-] [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000]。
功能仅输出 GCD，性能和事件独立文件；link 行表示该上升沿 valid=1 的 data，value 列为 ready(0/1)。连续 ready=0 的 link 行必须保留同一 id/a/b，下一个周期不能撤销 valid。compute_unit 的 a 为计算实例编号。

测试 tests/handshake/verify.py 检查 19 个有效场景（含极值、斐波那契、10 随机集、零延迟、慢输出）及 9 类非法输入，独立数值/精确延迟/沿前握手/保持/任务不丢不重/资源面积和峰值重算。无阻塞单任务接收4、完成10、输出11。

基本八项：78 周期，忙66周期，利用率84.62%，valid周期61，其中阻塞53；与作业1相同数值。该阻塞率分母是 valid周期，不是总周期。

资源代理：1 Compute、1 内部任务/结果槽、2 流水槽、4 FIFO槽。buffer_payload_bits=704 按 64位任务ID、32位操作数/GCD，统计 FIFO+流水有效载荷，不含有效位、指针、计算状态、除法器、互连或物理开销，也不计调试软件对象。它是声明位宽下的容量比较，不是综合面积。

验证：Debug 全 CTest 3/3；Release 作业2通过。Release 原作业1写 build/stage1-tests/summary.csv 时出现 PermissionError，未断言锁定原因；改用独立输出目录 build/stage1-stage2-regression 执行同一回归全部通过，不修改模型或放宽测试。原始日志保留 evidence/stage2/release-initial.log。
证据目录保存 summary.csv、Debug 日志和基本实际输出/统计；本目录所在提交为作业2可复现版本。没有标记候选人已理解全部新增代码。
