# 如何查看仿真过程与停止提示

2026-09-06，依据当前代码及本地 SystemC 内核实现核对。

## 作业 1：先看事件 CSV

VS Code 的 SystemC: Stage1 (GDB) 启动配置已传入事件文件参数，运行后查看 build/debug/stage1-events.csv。
命令行如使用 build 下的 Release 程序，按 stage1-run.md 指定自己的 EVENTS 路径；用 - 时不会生成事件文件。
CSV 列为 id,event,cycle,a,b,value,latency：id 为任务编号，cycle 为上升沿编号；当前时钟 1 ns，首沿 1 ns，所以 cycle 数字等于该时刻 ns 数。

| event | 含义 |
|---|---|
| parser_send | 解析模块向 FIFO 写入 |
| transform_accept | 变换模块接收 |
| transform_emit | 变换两级结束，向 FIFO 写入 |
| compute_accept | 计算接收，记录规范化操作数、预计结果、规定延迟 |
| compute_complete | 按规定延迟达到完成时刻 |
| compute_emit | 结果成功写入输出 FIFO |
| output | 输出模块读取并写功能文件 |

注意 compute_accept 中 value 是仿真 C++ 提前计算出的预测数值，不表示硬件已经完成；需到 compute_complete 才完成。
未使用字段的 0 是日志占位，不表示真实输入或结果必然为零。
同一时刻多个模块事件的 CSV 行顺序是软件回调记录顺序，不表示硬件中有先后额外周期；按 id 和 cycle 解读。
CSV 是任务关键事件记录，不是每拍所有寄存器快照。

单任务 48 18 的现有证据在 evidence/stage1/single.events.csv：1 发送、2 变换接收、4 变换写出、5 计算接收、11 完成/写出、12 输出。
性能汇总在 stage1-stats.csv，纯功能结果在 stage1-output.txt；后者不用于观察内部过程。

## 逐行观察

在 Parser::tick、Transform::tick、Compute::tick 或 Output::tick 断点，F5 运行，F10 单步。
查看 m_sent、m_stage1/m_stage2、m_state/m_remaining 等模块状态；代码入口及操作步骤见 vscode-debug.md。
tick 内单步是软件执行过程，信号/FIFO 本沿写入仍遵守延迟更新，不应把单步次数当作硬件周期。

## 波形

Day01 寄存器示例使用 VCD，VS Code 配置运行后为 build/debug/day01_basics.vcd，可交给支持 VCD 的波形查看器。
作业 1 当前只提供任务事件 CSV，没有添加完整 FIFO/状态 VCD，不能把 Day01 波形误当成作业 1 波形。
本轮没有安装波形查看器或新增观测代码。

## 停止提示

`Info: /OSCI/SystemC: Simulation stopped by user.` 来自 SystemC 内核 sc_simcontext.cpp 的 do_sc_stop_action。
这里 user 指仿真应用代码调用 sc_stop，不一定是用户手动按停止；Info 是信息等级，不是错误。
Day01 在检查结束后调用 sc_stop；Stage1 在 EOF、输出条数匹配且所有在途资源排空后调用 sc_stop。
因此当前程序正常运行结束会显示这条信息。由于时钟持续产生事件，需要明确停止。
该信息本身不证明功能正确，结合 PASS、退出码 0、结果/统计与测试判断；若提前结束或结果缺失，应检查停止条件。

本页为解释，未重新运行测试；具体运行是否成功应以用户那一次的完整输出和退出码为准。

## Day01 VCD 实读与查看（2026-09-06）

VCD（Value Change Dump）是信号值变化的纯文本记录。src/day01_basics.cpp 中 sc_create_vcd_trace_file 创建文件、sc_trace 注册四个信号、sc_close_vcd_trace_file 关闭文件。相对文件名写入进程工作目录；VS Code Debug 配置下为 build/debug/day01_basics.vcd。

本次实读：timescale=1 ns；aaaaa=clk，aaaab=tb_to_registers_data，aaaac=stage1，aaaad=registers_to_tb_data。`#15` 表示 15 ns；`b10100 aaaac` 表示 stage1 变为十进制 20。不重复记录未变化的信号。

| 时间/ns | clk | 输入 | stage1 | 输出 |
|---|---|---|---|---|
| 0 | 0 | 10 | 0 | 0 |
| 5 | 1 | 10 | 10 | 0 |
| 10 | 0 | 20 | 10 | 0 |
| 15 | 1 | 20 | 20 | 10 |
| 20 | 0 | 30 | 20 | 10 |

当前文件最后仅有 #25，无后续变化行，不能以文件证明 25 ns 输出已更新为 20；代码确实安排了该检查并在同一时刻 sc_stop。库 sc_vcd_trace.cpp 中关闭析构可仅补最后时间戳，默认未启用 delta tracing。停止/trace 刷新时序是末沿缺失的解释方向，本轮没有重新运行或修改代码验证修复。观察 5→15 ns 即可看到 stage2 读取 stage1 旧值；两次采样沿之间为一个 10 ns 周期。

可用 GTKWave：安装并加入 PATH 后运行 `gtkwave D:/0_lzh/GPU/MOORE/build/debug/day01_basics.vcd`，在 SystemC 层级选择四个信号并 Append，使用 Zoom Fit/Zoom Full，数据总线设 Data Format → Decimal 或 Signed。官方依据：[信号添加](https://gtkwave.github.io/gtkwave/quickstart/launching.html)、[菜单](https://gtkwave.github.io/gtkwave/ui/menu.html)。本机 PATH 未找到 gtkwave，不等于确认全机未安装；本轮没有安装或启动该查看器。VS Code 直接打开可读原文。现有 Python trace_tui.py 仅支持 Stage 1 CSV，不支持 VCD。
