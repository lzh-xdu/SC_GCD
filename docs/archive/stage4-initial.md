# 作业 4 初始版本

2026-09-09；用户明确选择：先将作业 1～3 所需源码整合到 `src/stage4/`，保持功能，再开始事件模型改造。

后续状态（2026-09-10）：以下保留初始提交 `1f252ab` 的历史说明；当前工作版本已开始 [Parser 事件迁移](stage4-parser.md)，事件日志比较允许同拍跨进程行序变化，不能再把当前全部日志描述为逐字节一致。

## 基线与范围

- 来源：`a52e13c1b47641a818836ef5982ca4af698fe999` 的作业 3 择闲调度、有限重排窗口版本（`stage3_window_gcd`）。
- 初始版本：本条所在提交，独立目标 `stage4_gcd`；目前仍为逐周期 SystemC 模型。
- 原作业 1～3、`src/common/` 和 `src/model/` 未修改；新目标仅链接 SystemC，不链接旧阶段的项目库。
- 不记为作业 4 事件模型完成，也不替代此前“待用户理解审阅”的状态。

## 源码对应

| 原文件 | 新位置与职责 |
|---|---|
| `src/stage1/{types,parser,output}.{hpp,cpp}` | `src/stage4/`，任务类型、输入和输出 |
| `src/stage2/{payload,transform,compute}.{hpp,cpp}` | `src/stage4/`，握手数据、变换和计算 |
| `src/stage3_window/{dispatcher,collector}.{hpp,cpp}`、`main.cpp` | `src/stage4/`，择闲分发、有限窗口收集和顶层连接 |
| `src/model/` | `src/stage4/model/`，独立数值、时序与观测实现 |
| `src/common/` | `src/stage4/common/`，独立契约与任务统计实现 |

共 30 个源码/头文件。只调整 include 路径、阶段命名空间（统一 `stage4`，模型使用 `stage4::model`）、冗余 using 和入口说明。未复制旧阶段的其他 main、FIFO Transform/Compute 或旧轮转 Dispatcher，避免引入不参与最终拓扑的重复模块。原有解释注释保留。

## 保持的约定

继承 [作业 3 窗口设计](stage3-window.md)、[模型分层](model-structure.md) 和 [任务统计](task-statistics.md)：上升沿握手、背压保持、计算延迟、择闲随机种子、窗口信用、按原序输出以及观察边界均保持。命令行位置和默认值与 `stage3_window_gcd` 相同，仅用法提示改为新目标名称。

```powershell
cmake --build build --target stage4_gcd --parallel 4
ctest --test-dir build -R stage4_initial_equivalence --output-on-failure
```

## 验证

Release/Debug 均构建成功、全量 CTest 13/13 通过。每种配置对原模型与新模型分别运行 31 个有效场景及 9 类非法输入，包含 Trace 开关检查、窗口 1/2/8、种子 1/7/42、背压、乱序完成和有序输出；两者各 276 份生成文件逐字节一致（包含输入、结果、事件、统计及汇总，不代表 276 个独立测试场景）。

记录见 [初始版本验证](../evidence/stage4-initial/validation.md)。本轮未测量主机仿真提速，未产生作业 4 的真实缺陷案例。下一步先明确事件唤醒与同拍行为，再逐个模块改造；初始版本提交作为后续对照点。
