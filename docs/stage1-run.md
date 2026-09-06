# 作业 1：运行与验证

本实现为待用户逐模块审阅的第一版；接口与时序见 [设计规格](stage1-design.md)。

## 文件阅读顺序

事件 CSV 图形展示见 [终端查看器](trace-viewer.md)：`python -B scripts/trace_tui.py build/debug/stage1-events.csv`。

1. src/stage1/types.hpp：共享消息类型、时钟单位及测试事件观察器。
2. src/stage1/parser.hpp/.cpp、transform.hpp/.cpp、compute.hpp/.cpp、output.hpp/.cpp：分别阅读模块头文件中的接口/缓存/时序图，再读对应 tick 行为。
3. src/stage1/main.cpp：连接、结束检测和统计；System 是顶层容器/监测，不是额外的数据处理阶段。
4. tests/stage1/verify.py：独立功能参考、时间线和资源统计检查。

## 构建和测试

需要 Python 3 解释器用于黑盒测试（本机 CMake 找到 C:/Python314/python.exe）；模型本身仍是 C++/SystemC。可用 -DPython3_EXECUTABLE 指定其他解释器。

```powershell
./scripts/build-test.ps1
```

完整验证包括原来的 day01 示例与新增 stage1 测试，共 2 个 CTest 入口。

## 运行题目八组输入

从项目根目录运行，build 目录由构建产生：

```powershell
./build/stage1_gcd.exe ./tests/stage1/basic.txt ./build/stage1-output.txt ./build/stage1-stats.csv 2 ./build/stage1-events.csv
```

命令行格式：

```text
stage1_gcd INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-] [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000]
```

- OUTPUT：只有每行一个 GCD。
- STATS：指标 CSV，独立于功能输出。
- EVENTS：可选逐任务事件 CSV；用 - 关闭。
- DEPTH：三个 FIFO 各自的容量。
- OUTPUT_PERIOD：默认每拍可接收；增大用于制造下游阻塞，不代表计算变快。
- MAX_CYCLES：模拟超时；可增大以运行更多数据。
- 所有输入/输出路径必须不同，父目录应已存在；异常返回非零，部分输出不能当作完成结果。

## 本次实际结果

以下为首版结果快照；2026-09-06 最新扩展覆盖与输出分离验收见 [测试矩阵](stage1-test-matrix.md)。新增 10 组随机集、连续斐波那契、极值组合及短长混合，实际结果见 evidence/stage1-expanded。

- 题目八组：6、25、1、48、1、1、7、1。
- 深度 2、输出周期 1：79 周期、8 任务、有效计算 66 周期、利用率约 83.54%、吞吐率约 0.1013 任务/周期。
- 单任务 48 18：第 2 沿变换接收、第 4 沿写出、第 5 沿计算接收、第 11 沿完成、第 12 沿输出。
- 宽 FIFO、20 个零除数任务：变换接收沿 2～21、写出沿 4～23，验证基础延迟两周期且每周期一条。
- 24 个 (30,30)、容量 1、输出每 13 拍：312 周期，结果写出因 FIFO 满失败的周期数为 248；数值/任务数/顺序保持正确。
- 两组相同种子的 250 随机任务（容量 1，以及容量 4/输出周期 3）均通过；不推断更大缓冲必然更快。
- 覆盖空输入、32 位边界、负数、重复任务、6 类非法行和模拟超时。

统计由测试根据事件重新计算占用面积/峰值，功能使用 Python math.gcd；计算延迟还核对手工单任务时间线。
这些结果是本配置下的验证证据，不是全输入形式化证明，不表示用户已经掌握全部代码。

证据见 [evidence/stage1](evidence/stage1/)。首次预期文件错误及修正见 [B-003](defects.md)。
