# SystemC 架构与仿真实践

以 GCD 流式处理器为载体，学习 SystemC 建模、架构设计与仿真优化。通过逐步演进模型、验证时序、比较性能，记录设计取舍、调试过程与理解的成长。

## 探索方向

- **流式架构**：从单实例流水线到显式握手、背压控制，再到双实例调度与保序输出。
- **模型分层**：分离功能行为、时序行为和观测统计，让模型更易理解、验证与演进。
- **仿真优化**：以逐周期模型为基线，探索事件驱动建模，通过等价验证与实测评估收益。
- **问题复盘**：保留真实缺陷、复现输入、修正过程和回归证据，让每次改进有据可查。

当前已建立流水线、握手及双实例保序模型；事件驱动模型与仿真提速验证仍在探索中。详见[项目状态](docs/status.md)。

## 快速开始

已验证环境：Windows x64、MinGW GCC 13.2.0、CMake 3.29.2、Ninja、C++17、SystemC 3.0.1。需将 Git、编译工具和 Python 3 加入 PATH；无需 GPU。

在项目目录的 PowerShell 中运行：

```powershell
# 首次下载固定版本的 SystemC，需要联网
./scripts/setup.ps1

# 编译并运行自动测试
./scripts/build-test.ps1
```

SystemC 与模型使用同一编译器构建，无需预装系统级库。模型运行示例见[流水线](docs/stage1-run.md)、[握手](docs/stage2-run.md)和[双实例保序](docs/stage3-run.md)。

## 文档

- **理解模型**：[SystemC 入门](docs/day01.md) · [架构设计](docs/final-design.md) · [职责分层](docs/model-structure.md)
- **观察与优化**：[Trace 可视化](docs/trace-viewer.md) · [任务统计](docs/task-statistics.md) · [架构优化分析](docs/stages1-3-optimization-review.md)
- **学习与复盘**：[学习记录](docs/learning-log.md) · [设计决策](docs/decisions.md) · [缺陷记录](docs/defects.md)
- **持续演进**：[工作日志](docs/work-log.md) · [AI 协作记录](docs/ai-log.md) · [记录方式](docs/process.md)

## 依赖

[Accellera SystemC](https://github.com/accellera-official/systemc) 固定为 3.0.1，采用 Apache-2.0 许可；详见依赖源码中的 LICENSE 与 NOTICE。
