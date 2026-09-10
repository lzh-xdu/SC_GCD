# SystemC 架构与仿真实践

[![CI](https://github.com/lzh-xdu/SC_GCD/actions/workflows/ci.yml/badge.svg)](https://github.com/lzh-xdu/SC_GCD/actions/workflows/ci.yml)

以 GCD 流式处理器为载体，学习 SystemC 建模、架构设计与仿真优化。通过逐步演进模型、验证时序、比较性能，记录设计取舍、调试过程与理解的成长。

## 探索方向

- **流式架构**：从单实例流水线到显式握手、背压控制，再到双实例调度与保序输出。
- **模型分层**：分离功能行为、时序行为和观测统计，让模型更易理解、验证与演进。
- **仿真优化**：以逐周期模型为基线，探索事件驱动建模，通过等价验证与实测评估收益。
- **问题复盘**：保留真实缺陷、复现输入、修正过程和回归证据，让每次改进有据可查。

当前已建立流水线、握手及双实例保序模型；事件驱动模型与仿真提速验证仍在探索中。详见[项目状态](docs/status.md)。

## 快速开始

支持 Windows 和 Linux 两套编译运行环境，均已验证：

- **Windows x64**：PowerShell、MinGW GCC 13.2.0、CMake 3.29.2、Ninja，产物目录 `build`。
- **Linux x64**：bash、GCC 11.4（WSL Ubuntu 22.04 验证）、CMake 3.22+、Unix Makefiles，产物目录 `build-linux`。

两套环境均需 Git、C++17 编译器和 Python 3 在 PATH 中，无需 GPU；SystemC 与模型用同一编译器构建，无需预装系统级库。

**Windows（PowerShell）**

```powershell
# 首次下载固定版本的 SystemC，需要联网
./scripts/setup.ps1

# 编译并运行自动测试（默认 Release；Debug 加 -Configuration Debug -BuildDirectory build\debug）
./scripts/build-test.ps1
```

**Linux / WSL（bash）**

```bash
# 首次下载固定版本的 SystemC，需要联网（等同 setup.ps1）
git clone --depth 1 --branch 3.0.1 \
    https://github.com/accellera-official/systemc.git third_party/systemc

# 配置、编译并运行自动测试（Debug 换 -DCMAKE_BUILD_TYPE=Debug 及独立目录）
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-linux --parallel 4
ctest --test-dir build-linux --output-on-failure
```

两套环境共用同一源码和测试，产物目录均已加入 .gitignore；双环境完整验证日志见 [docs/evidence/dual-environment/](docs/evidence/dual-environment/)。模型运行示例见[流水线](docs/archive/stage1-run.md)、[握手](docs/archive/stage2-run.md)和[双实例保序](docs/archive/stage3-run.md)。

## 文档

- **核心**：[最终设计](docs/final-design.md) · [实测结果](docs/metrics-results.md) · [项目状态](docs/status.md)
- **协作与过程**：[AI 协作记录](docs/ai-log.md)（设计要求/方案修改/问题处理三方面） · [工作日志](docs/work-log.md) · [学习记录](docs/learning-log.md) · [记录方式](docs/process.md)
- **历史归档**：各阶段设计规格、双模型对比、工具说明等见 [docs/archive/](docs/archive/)

## 依赖与许可

本项目代码采用 [MIT 许可](LICENSE)（2026 SC_GCD contributors）。

[Accellera SystemC](https://github.com/accellera-official/systemc) 固定为 3.0.1，采用 Apache-2.0 许可；详见依赖源码中的 LICENSE 与 NOTICE。

## 里程碑

四个作业各自验收通过时打有 git 标签：`stage1`、`stage2`、`stage3`、`stage4`（见 `git tag -l -n`）。
