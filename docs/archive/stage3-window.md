# 作业 3 优化：择闲派发与滑动窗口

2026-09-06～07；用户提出：都空闲随机选，仅一个可用就选它，短任务结果先缓存、最终按编号输出。旧方案保留为stage3_gcd，新方案为stage3_window_gcd。

## 设计与用户方案的细化

Parser→FIFO(D)→两级Transform→择闲Dispatcher→两个Compute→各自FIFO(R)→W槽Collector→FIFO(D)→Output。

- 任务id原本已由Parser递增分配，直接沿用；Compute的IDLE/BUSY/RESULT_PENDING也已存在，不增加第四种状态。
- Dispatcher只考虑ready=true的单元；两者都ready时用可复现的xorshift32选择，随机状态只在成功的双空闲选择沿推进。默认seed=1，允许1～4294967295；没有在组合方法反复执行时抽随机数。
- 无单元可接收时向上游ready=false；两者都可接收则选state%2。只向可接收端给valid，所有状态变更仍在上升沿，组合路由在delta间稳定。
- Collector用固定W个optional<Result>，索引id%W，并检查完整id；base指向下一项待写输出FIFO的编号。每沿至多收一项、发一项，先发旧槽、再收新结果；因此新增一拍结果存储延迟，收取带宽与旧方案同为每拍一条。
- 结果输入择可读通道，双方都有结果时轮流取，避免长期偏向一路；这与计算任务的随机择闲策略分开。

不能简单采用“缓存被后续结果填满后，所有结果都不再取”：最早结果如果仍在Compute/FIFO外侧，将没有机会进入缓存，形成循环等待。这是对用户草案的设计风险分析，**没有声称旧实现实际发生过该死锁**。

本实现提前预留窗口槽：只有id在[base,base+W)才允许派发，已经派发的任务均有唯一槽，包括尚未完成的长任务。窗口预留满时停止新派发，但继续接收已预留任务的结果，特别是最早项。base更新到信号后，下一个上升沿才能使用释放的credit。

这比只限制“已完成结果数量”更保守：W限制所有已派发但未进入输出FIFO的任务。需要更深前瞻时增大W；不会以无限队列换性能。窗口满可能在Compute仍空闲时阻止新任务，而不是必然让Compute停在RESULT_PENDING。该状态仍用于结果FIFO不能写入的情况。

## 旧方案先测与公平比较

基线源码ad4b33e，新代码修改前先运行Release旧程序，保存32次运行：16组输入×R=2/6，D=2，事件关闭。manifest记录旧可执行文件SHA256，各输入也记录SHA256。原始结果见evidence/stage3-window/baseline。

新方案同输入、D=2、R=2、W=8，在seed=1/7/42/20260906上运行64次，另有head_long的W=1/2/4/16扫描。四个种子的本批总周期相同，不推广为所有输入都与种子无关。逐例核对math.gcd、输入摘要、有效计算量和输出节拍一致。

| 测试 | 旧轮转R=2 | 旧轮转R=6 | 新择闲W=8 | 相对旧R=2 |
|---|---:|---:|---:|---:|
| 基本8项 | 57 | 57 | 47 | 1.213倍 |
| 连续斐波那契 | 560 | 560 | 561 | 0.998倍 |
| 短任务夹长任务 | 264 | 264 | 257 | 1.027倍 |
| 长短突发，输出每3拍 | 648 | 648 | 375 | 1.728倍 |
| 偶数长/奇数短偏斜 | 906 | 906 | 470 | 1.928倍 |
| 首项长、后40项短 | 120 | 120 | 121 | 0.992倍 |
| random_01 | 1561 | 1561 | 1540 | 1.014倍 |

其余9个随机集结果全部保存，未只挑有利数据。旧R=6与新W=8的声明缓冲载荷均1856bit：旧16个FIFO槽；新8个FIFO槽+8个窗口槽。旧默认R=2为1088bit。增加旧缓存并没有消除本批严格轮转瓶颈，说明主要改善来自调度/保序结构变化；但这仍不是等面积比较，新窗口有效位、控制和随机状态有额外成本。

## 为什么首项长不一定缩短总时间

head_long两版首项都在第78拍完成。此前旧版只完成了1项后续短任务，新W=8版已完成7项；用户期望的提前计算确实发生。最终仍需每拍一项顺序输出，旧版在首项完成后也能按该速率供给结果，所以整体120→121，多的一拍来自新窗口。

同输入窗口扫描：W=1为241周期，W=2为161，W=4/8/16均121。大窗口只在能覆盖瓶颈时有收益，不能假定越大越快。

## 使用与统计

```powershell
./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin
./build/debug/stage3_window_gcd.exe tests/stage1/basic.txt build/debug/window-output.txt build/debug/window-stats.csv 2 build/debug/window-events.csv 1 1000000 2 8 1
```

参数为INPUT OUTPUT STATS [D=2] [EVENTS|-] [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000] [R=2] [W=8] [SEED=1]。VSCode新增Stage3 Window的运行/调试配置，旧Stage3仍可用。

新增window_blocked_cycles、engines_blocked_cycles，两者互斥并合计为握手阻塞；窗口不足优先计入前者。window_results峰值/均值是已缓存结果，window_reserved峰值/均值是已派发未退出窗口的任务。base在Collector写输出FIFO时前进，尚未被Output读取的任务不再占窗口，但仍占下游FIFO。

window_store表示结果进入窗口，reorder_emit表示顺序退出；random_choice记录选择及随机状态。载荷容量仍只比较逻辑有效载荷，另报W个窗口有效位，不填真实面积/功耗。

## 验证与复现

Debug全CTest6/6；Release相关5/5，旧作业1换独立输出目录24场景/1434项通过（既有路径权限问题沿用之前处理，不改判定）。新模型31个有效场景：原19、zero_burst、9组头阻塞窗口/种子组合、慢输出与同种子重跑，另有9类非法输入。

检查独立GCD、任务唯一/保序、每单元完成沿限制、择闲及随机序列、窗口编号范围/容量、环形复用、窗口/FIFO时序、统计重算；W=1/2/8满窗口仍能排空，固定种子事件逐字节相同。zero_burst实测覆盖RESULT_PENDING。

首轮照搬旧慢输出断言失败，定位和修正见B-008；未放松数值/时序检查，也未编造硬件故障。Release/Debug新模型统计一致。

```powershell
python tests/stage3/benchmark_window.py baseline build/stage3_gcd.exe build/window-baseline
python tests/stage3/benchmark_window.py window build/stage3_window_gcd.exe build/window-optimized
python tests/stage3/summarize_window.py build/window-baseline/summary.csv build/window-optimized/summary.csv build/window-comparison.csv
```

上述比较的是模拟系统性能，未涉及作业4或主机仿真效率。
