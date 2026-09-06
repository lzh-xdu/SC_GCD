# 终端查看器验证

日期：2026-09-06。基线 8009f6f；新增查看器、测试和文档在本条所在提交中。环境 Python 3.14.4 / Windows PowerShell。

- `python -B -m unittest discover -s tests/stage1 -p test_trace_tui.py -v`：5/5 通过，退出 0。覆盖半开区间、零延迟、结果背压、Transform 延长、空/部分 trace、逆序拒绝、跳转、平移保持、缩放、播放与退出控制。
- `python -B scripts/trace_tui.py --snapshot --cycle 13`：退出 0，输入为用户的 build/debug/stage1-events.csv，见 [静态帧](trace-viewer-snapshot.txt)。66 忙周期、8 任务、79 事件周期、83.54% 利用率，与本次相邻 stage1-stats.csv 对应字段人工核对一致。
- Windows 伪终端实际启动，观察彩色时间线；发送 d、n、s、j、-、+、f、空格，观察周期推进、任务切换、缩放与 PLAY 自动推进；q 退出 0 并恢复终端。
- 首次 80×24 伪终端触发原 100×28 最小尺寸提示，调整支持 80×24 后复测通过。窄窗口会截短文字，完整阅读推荐 140×40。未进行原生窗口像素级截图检查。
- 无 C++ 改动，未重复运行 CTest；工具测试不替代模型验收。
