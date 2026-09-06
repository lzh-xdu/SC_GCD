# SystemC 流式处理器面试作业

当前进度：第 1 天环境与仿真语义实验。尚未实现作业 1 的 GCD 流式处理器。

## 编译、测试和运行

已验证的工具组合：Windows x64、MinGW GCC 13.2.0、CMake 3.29.2、Ninja、C++17、SystemC 3.0.1。
编译工具需在 PATH 中；本机 GCC/CMake/Ninja 来自 `C:/Strawberry/c/bin`。
不需要 GPU，也不需要安装系统级 SystemC 库。

在项目目录的 PowerShell 中运行：

```powershell
# 首次获取依赖；当前工作目录已经准备好，换机器时需联网执行。
./scripts/setup.ps1

# 编译、自动测试，并显示示例输出。
./scripts/build-test.ps1 -Run

# 只重新运行测试。
ctest --test-dir build --output-on-failure
```

若 PowerShell 提示脚本执行策略阻止运行，可仅对本次进程使用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/build-test.ps1 -Run
```

跨平台 CMake 工程也可直接构建（自行选择已安装的编译器）：

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

首次构建会编译 SystemC，后续只重编译修改部分。不要在同一个 build 目录切换编译器。
Windows 下单独启动 exe 时，需保留所用 MinGW 编译器的 bin 目录在 PATH 中，以加载其运行库。

## 第一个实验

阅读 `src/day01_basics.cpp`，然后阅读 `docs/day01.md`。
程序检查三个上升沿前后的信号值，失败时返回非零退出码，CTest 将报告失败。
运行生成 `build/day01_basics.vcd`，可供后续波形查看；当前无需安装波形工具。

## 目录

- `src/`：学习示例，之后添加各阶段模型。
- `scripts/`：依赖准备与构建测试入口。
- `docs/`：学习约定和真实工作记录。
- `third_party/systemc/`：官方依赖源码，忽略于项目版本控制。
- `build/`：构建产物、测试日志和波形，忽略于版本控制。

## 依赖来源

官方仓库：https://github.com/accellera-official/systemc

固定版本：3.0.1，提交 `11ad094d282fd5330b27ab57f90f9d231a763da1`。
依赖以 Apache-2.0 授权，详见其 LICENSE 与 NOTICE。
使用同一编译器及 C++17 同时构建库和示例，避免混用二进制 ABI。

## 后续完成标准

过程记录入口：[记录方式](docs/process.md)、[阶段状态](docs/status.md)。后续协作按根目录 AGENTS.md 及时沉淀设计、验证、AI 协作与缺陷证据。

依次完成单实例流水、显式握手、双实例保序，再尝试事件驱动等价模型。
每天记录设计、AI 协作、修改、调试与验证。公开仓库发布尚未进行。
