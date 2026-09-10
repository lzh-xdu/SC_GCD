# 作业 2：显式握手接口提案

2026-09-06：用户询问能否进入开发，并提出 `if (!m_computReady || !m_tasksIn.nb_read(task))`。
本页为接口澄清与下一步设计，尚未实现作业 2，不覆盖作业 1。

后续状态：用户授权后已按stage2-design.md实施并在75c76f5独立验收；本页保留为原始提案，当前接口及结果见stage2-run.md。

## FIFO 检查与显式握手的区别

用户表达了“下游未准备好则不取新数据”的正确方向。C++ 的 || 短路保证 ready 为 false 时不调用 nb_read。
但该表达式仍由 sc_fifo::nb_read 完成接收，valid 和 data 并未成为边界上的显式信号，因此仅这样修改不满足题目。
也不能直接用 Compute 是否空闲限制 Transform 从 Parser 接收：只要 Transform 内部有空位，它仍可预先接收；背压应逐级传播。

## 要替换的边界

Parser --有限 FIFO--> Transform --data/valid--> Compute --有限 FIFO--> Output
                                <--ready-----

仅替换 Transform 到 Compute 的连接。Compute 接收处不再从旧 FIFO nb_read。
初步采用 Transform 排序级作为该接口唯一待发送槽，保留绝对值级；原边界外部 FIFO 移除。
这会改变缓冲配置和某些时间点，不假定作业 1/2 性能完全一致；跨版本比较需说明配置差异。
以上缓冲选择是 AI 建议，后续实现前需明确记录最终约定。

## 端口提案

Transform：sc_out<OrderedTask> m_dataOut；sc_out<bool> m_validOut；sc_in<bool> m_readyIn。
Compute：sc_in<OrderedTask> m_dataIn；sc_in<bool> m_validIn；sc_out<bool> m_readyOut。
顶层用对应 sc_signal 绑定，data/valid 正向、ready 反向。
若 sc_signal<OrderedTask> 要求比较操作，则为任务类型补充值比较；不能绕过类型要求使用共享裸变量。

## 接收与保持规则

每个上升沿两端根据本沿可见的 valid && ready 判断同一次传输。
Compute 接收伪代码：若 !(m_validIn.read() && m_readyOut.read()) 则不接收，否则复制 m_dataIn.read() 并开始任务。
这段代码只属于可接收分支；BUSY/RESULT_PENDING 仍要推进计算和处理结果，不能放在 tick 顶部使忙碌状态提前返回。
Transform：valid=1 且 ready=0 时保持 valid 和 data；握手后旧槽才可释放，随后可把前级任务移入，更新供下一沿使用的信号。
ready 必须由 Compute 统一驱动，提前反映接收能力。完成沿禁止接收下一任务；本沿写 ready=true 到更新阶段生效，最早下一上升沿握手。
初始化值、FIFO 改直连接口后的两周期时序、同拍发送/补入和零延迟行为需要逐沿表验证。

## 下一步验收

- 先给单任务、连续任务、阻塞恢复画逐沿表，再实现独立作业 2 目标。
- 固定 ready=0 的连续等待：valid/data 不变，接收数不增加。
- 重复相同任务必须按握手次数计数，而非数值变化次数。
- 长短延迟、完成当拍禁止接收、有限槽位、小缓冲及输出阻塞继续验证。
- 增加 valid && !ready 的阻塞周期统计及相应定义；保留作业 1 的独立构建与测试。
