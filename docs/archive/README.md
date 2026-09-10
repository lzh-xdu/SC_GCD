# 历史文档归档

2026-09-11 文档精简融合时移入。这些文档的内容已按主题融合进顶层文档（[ai-log.md](../ai-log.md) 协作三方面、[final-design.md](../final-design.md) 技术设计、[metrics-results.md](../metrics-results.md) 实测结果），或属于特定轮次的运行/验证记录，不再更新；保留原文以供追溯，Git 历史含全部变更过程。

| 归档文档 | 内容 | 主要去向 |
|---|---|---|
| decisions.md | 设计决策 D-001~D-026 全文 | 协作相关条目摘入 ai-log.md；技术选择已入 final-design.md |
| defects.md | 缺陷 B-001~B-013 全文 | ai-log.md §三（含作业 3/4 主动案例 B-006/B-011） |
| review-fixes.md | 三轮用户审查修复记录 | ai-log.md §三 |
| engineering-audit.md | 原题 6.6 逐项审核 | ai-log.md §三处理原则；完成状态见 status.md |
| stage1~4 各设计/运行/验证文档 | 各阶段接口规格、时序契约、测试矩阵与运行方法 | final-design.md §2~§3 已压缩保留关键契约；细节见原文 |
| metrics-presentation.md | 四作业统计指标与呈现设计 | metrics-results.md（已按其执行实测） |
| model-comparison.md | Windows 入口径双模型实测报告 | metrics-results.md §4 摘要＋引用 |
| task-statistics.md、model-structure.md、code-contracts.md | 统计口径、分层结构、契约风格说明 | final-design.md §2~§3；coding-style.md |
| master-plan.md、inspect-simulation.md | 初始计划与检查清单 | status.md 反映当前状态 |
| day01*.md、vscode-debug.md、trace-viewer.md、web-viewer.md | 入门教学与工具说明 | 原文保留（工具仍可用） |
| fifo-handshake-evidence.md | FIFO→握手替换的性能依据核查 | metrics-results.md §2 相关结论 |
| stages1-3-optimization-review.md | 58 组配置优化探测 | 结论未采纳为默认参数，保留探测数据 |

归档文档内部指向顶层文档与 evidence/ 的链接已修正为相对路径；文档之间的互链保持原样。
