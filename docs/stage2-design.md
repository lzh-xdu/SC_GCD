# 作业 2：显式握手设计

2026-09-06，按用户授权实施；先独立验收，再扩展作业 3。

Parser → 有限 FIFO(D) → Transform 两级 → valid/ready/data → Compute → 有限 FIFO(D) → Output。
Parser、Output、消息和测试日志复用作业 1；新握手模块独立文件，保留 stage1_gcd 可复现。

Transform 的排序槽直接驱动 data/valid，不另设无限队列。上升沿读取旧 valid&&ready 才移除任务；未握手则保持槽及信号。逆序移动绝对值槽、再接收上游，每拍最多一项，接收 k 到最早握手 k+2。sc_signal 延迟更新使各模块看到相同的沿前接口值。

Compute 初始 ready=true；仅空闲时接受握手。L>0 时 k+L 完成，完成沿不接新任务；若结果成功写 FIFO，更新 ready，最早下一沿接收。结果 FIFO 满时保存结果且 ready=false。L=0 延续作业 1 约定，同一沿完成当前任务但不接受第二项。

统计继承周期、任务、忙周期、吞吐、利用率和资源占用，并增加发送有效周期、valid&&!ready 阻塞周期、阻塞率及结果等待周期。握手统计在沿前值上计数，资源占用在 delta 更新后采样。可选逐拍 link 记录用于独立检查保持与接收规则。

资源指标为结构代理：Compute 数量、任务/结果缓冲槽数、按约定位宽计算的逻辑载荷容量；不将 sizeof(C++ 对象) 当作硅面积，不生成无依据的 mm²/W 指标。面积/功耗需后续 RTL/综合工艺/频率和活动数据。

与作业 1 的比较需注明：原 Transform→Compute 外部 FIFO 被移除，排序槽即握手保持槽，因此总缓冲数量不同；不能把所有周期差都归因于握手协议。

本设计替代 stage2-interface-proposal.md 的待定项，验证结果另记运行文档和证据。
