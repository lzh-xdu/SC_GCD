# Stage 1 终端 trace 查看器

在项目根目录的 PowerShell / Windows Terminal 中运行（Python 3，无第三方依赖）：

```powershell
python -B scripts/trace_tui.py build/debug/stage1-events.csv
```

建议终端 **140 列、40 行**，同时查看全部 8 条任务和说明。最小支持 80×24，小窗口截短文字并减少每页任务数。英文、ASCII 图格避免中文宽度和特殊字体导致时间轴错位。退出恢复终端和光标。

| 按键 | 操作 |
|---|---|
| a / d | 前一 / 后一周期 |
| p / n | 前一个 / 后一个有事件的周期 |
| w / s | 上一 / 下一任务，自动翻页 |
| j | 跳到选中任务发送周期 |
| h / l | 时间窗口左右平移 |
| + / - | 放大 / 缩小 |
| f | 全程适配窗口 |
| 0 | 回到周期 0、每列一周期 |
| 空格 | 播放 / 暂停，每秒约 5 个模拟周期 |
| q | 退出 |

静态模式适用于不能交互的输出窗口，也可重定向保存一帧：

```powershell
python -B scripts/trace_tui.py --snapshot --cycle 13 --width 140
```

## 如何读图

横轴为模拟周期，每行一条任务；`>` 为选中任务，`^` 为当前周期。时间线包含完整历史，游标决定当前占用和事件说明；顶部指标始终汇总整个 trace。

| 字符 / 颜色 | 区间 | 含义 |
|---|---|---|
| F / 青 | parser_send → transform_accept | 输入 FIFO 驻留 |
| T / 蓝 | transform_accept → transform_emit | Transform 驻留，包含基础两周期与额外等待 |
| Q / 黄 | transform_emit → compute_accept | Compute 输入 FIFO 驻留 |
| C / 绿 | compute_accept → compute_complete | 有效计算 |
| B / 红 | compute_complete → compute_emit | 结果完成但无法交付 |
| O / 紫 | compute_emit → output | 输出 FIFO 驻留 |
| * / 白 | output 所在沿 | 输出完成事件 |
| . | 无上述活动 | 尚未发送或已输出 |

区间是 `[起点,终点)`，显示沿处理后的状态。同沿完成并写出的 B 长度为 0；零周期状态不占图格，Events 仍保留事件。缩小后每列采样左端周期，短事件可能隐藏；精确分析放大至一周期一列。

F/Q/O 包含 sc_fifo 固有的下一沿可读延迟，不能全算成可消除的阻塞。T 超过 2 的部分是本模型的额外驻留，不能由此定位两个内部寄存器各自的移动周期。非相关字段 0 是占位值；工具从 compute_accept 或 transform_emit 读取操作数，从 output 读取最终结果。

## 本次观察与效率边界

8 条任务全部输出，最后事件在 79 周期；有效计算 66 周期，利用率 83.54%，吞吐率 0.1013 任务/周期；结果等待 0 周期；平均端到端延迟 38.125 周期。

先用 n 看前几拍，再选择任务 3～7，可以看到 Q 和 T 变长。周期 13 沿后：输入 FIFO 1 条、Transform 2 条、Compute 输入 FIFO 2 条、Compute 1 条、待交付结果和输出 FIFO 均为 0。本次可先关注串行计算与上游积压，不代表其他输入下瓶颈相同，也不证明加深 FIFO 必然提速。

**模型效率与仿真器实际运行速度需区分。** 指标以最后事件周期为分母；本次与相邻 stats 的 cycles=79 一致，但工具不读取其他统计文件。截断 trace、空输入或 EOF 检测可能使事件窗口短于实际仿真区间。未输出任务不参与平均延迟；忙时间只累计到最后事件。允许合法事件前缀，拒绝错误表头、缺字段、未知/重复事件和任务内逆序。

CSV 无主机耗时、delta 次数、进程唤醒次数和 FIFO 容量，因此不能展示主机模拟速度、精确满载率或 Parser 每次失败发送。以后比较作业 4，需相同输入、构建配置和 trace 开关下重复测量实际耗时；播放速度不是仿真性能。本轮不提前实现后续阶段。

验证见 [验证记录](../evidence/stage1/trace-viewer-validation.md)。查看器只读输入，不改动模型。
