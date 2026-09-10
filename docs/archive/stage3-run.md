# 作业 3：运行、性能与资源取舍

2026-09-06；设计见 [严格轮转架构](stage3-design.md)，真实容量案例见 [B-006](defects.md)。

后续用户选择择闲+滑动窗口，新目标和比较见[优化方案](stage3-window.md)。本页保留旧轮转目标的可复现说明与结果。

```powershell
./scripts/build-test.ps1 -Configuration Debug -BuildDirectory build/debug -ToolchainBin C:/Strawberry/c/bin
./build/debug/stage3_gcd.exe tests/stage1/basic.txt build/debug/stage3-output.txt build/debug/stage3-stats.csv 2 build/debug/stage3-events.csv
```

参数：INPUT OUTPUT STATS [DEPTH=2] [EVENTS|-] [OUTPUT_PERIOD=1] [MAX_CYCLES=1000000] [RESULT_DEPTH=2]。
DEPTH 只控制 Parser→Transform 和 Collector→Output；RESULT_DEPTH 控制两条 Compute 结果FIFO，各自有相同容量。
输出仍纯GCD。事件除作业2项外增加 reorder_emit、reorder_wait、collector_blocked、dispatch_idle_other；等待事件允许同一id连续出现。原 trace_tui 面向作业1，未宣称已适配双单元；当前用CSV或调试器检查。

VS Code 新增 SystemC: Stage2/Stage3 (GDB) 和 Run Stage2/Stage3，复用 Build Debug。JSON与命令行构建验证通过；未直接操作GUI宣称F5已实测。

## 已执行的比较

以下两版本都使用D=2、默认时钟1ns，作业3使用R=2；同输入、同输出节拍、关闭事件日志。加速比=C2/C3，是模拟硬件吞吐的比较，不是主机执行速度。

| 输入 | 作业2周期 | 作业3周期 | 加速比 |
|---|---:|---:|---:|
| 基本8项 | 78 | 57 | 1.3684 |
| 连续斐波那契17项 | 1053 | 560 | 1.8804 |
| random_01，64项 | 3063 | 1561 | 1.9622 |
| 短任务夹长任务43项 | 305 | 264 | 1.1553 |
| 容量压力48项，输出每3拍 | 705 | 648 | 1.0880 |
| 严格轮转偏斜24项 | 928 | 906 | 1.0243 |

偏斜输入的偶数编号全部为74周期长任务、奇数编号为1周期短任务。任务数量各12条，但有效计算量为888对12周期；“轮流发同样多的任务”不能保证负载平衡。保留严格轮转并报告限制，择闲分配属于待用户选择的后续优化。

基本8项两个单元各忙33周期，各利用率33/57=57.89%；总利用率66/(2×57)=57.89%。不要用66/57误报为计算资源利用率。
Collector为保序多增加一段FIFO传递；单任务4拍接收、10拍完成、11拍合并、12拍最终输出，因此小负载不保证比作业2更快。

## 资源成本代理

| 默认配置指标 | 作业2 | 作业3 |
|---|---:|---:|
| Compute数 / 内部任务结果槽 | 1 / 1 | 2 / 2 |
| Transform槽 | 2 | 2 |
| 外部FIFO总槽数 | 4 | 8 |
| 声明的缓冲载荷容量（bit） | 704 | 1088 |

载荷按task=64位id+2×32位操作数、result=64位id+32位GCD，buffer_payload_bits只含FIFO和流水槽有效载荷，不包含有效位、FIFO指针、Compute状态/倒计时、算术电路及连线。信号镜像不重复算槽，C++对象实际字节数不当硬件面积。

comparison.csv 还给出 speedup_per_compute=加速比/2：随机集约0.9811，偏斜集约0.5121，可看出新增算力被利用的程度。这是性能/单元数代理，不能等同于性能/mm²或性能/W。
当前不填面积、功率、能耗为0或凭空给数；要得到可信PPA需具体RTL/HLS实现、工艺库、频率/电压、开关活动与综合/功耗分析或可靠标定。

## 验证与范围

- Debug全CTest 5/5：作业1、Day01、作业2握手、作业3保序、单/双性能比较。
- 作业3有22个有效场景（包含原19场景及容量前后/偏斜）和9类非法输入；数值、握手保持与ready容量、精确延迟、同沿限制、资源占用与统计均检查。
- Release新阶段/比较及Day01为4/4；作业1因既有汇总路径写权限问题使用独立目录，24场景/1434项仍通过。原始失败见作业2记录；未改测试判定绕过问题。
- Release/Debug阶段2/3统计汇总和comparison逐字节一致。实际输出/统计、容量案例前后事件和测试日志见 [证据](../evidence/stage3/README.md)。
- 基本输入在两个新阶段都核对了开启/关闭追踪的output与stats逐字节相同；C++格式检查和新增VSCode JSON解析通过。
- 作业4未实现，时钟驱动与事件驱动的主机仿真效率/开销比较仍待完成；本轮没有将本表冒充该项要求。
