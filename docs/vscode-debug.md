# VS Code 构建、运行与调试

使用“打开文件夹”打开 D:/0_lzh/GPU/MOORE。配置面向 Windows + 本机 MinGW GCC/GDB。
已有 Microsoft C/C++ 和 CMake Tools 扩展目录；无需再次安装。换机器时按 extensions.json 推荐安装 C/C++。

## 快捷操作

1. Ctrl+Shift+B：执行 SystemC: Build Debug，首次编译 Debug 版 SystemC，之后增量构建。
2. 在 src/stage1/parser.cpp 的 Parser::tick 内第一行设置断点（原 modules.cpp 已拆分）。
3. 运行和调试下拉框选 SystemC: Stage1 (GDB)，按 F5：自动先构建，再运行题目八组数据。
4. F10 单步跳过，F11 单步进入，Shift+F11 跳出，F5 继续；悬停或 Watch 查看 m_sent、m_eof、m_tasksOut。
5. Ctrl+F5：使用选中的启动配置运行而不调试。
6. Terminal → Run Task → SystemC: Test Debug：构建后运行完整 CTest。

Day01 的启动配置和运行任务也已提供。不要使用“仅编译活动文件”按钮来构建多文件 SystemC 工程。

## 配置文件

- launch.json：Stage1 和 Day01 两个 cppdbg/GDB 启动项。
- tasks.json：Debug 构建、Debug 测试、两个示例运行；任务互相依赖，运行前确保程序存在。
- c_cpp_properties.json：C++17、GCC、Debug compile_commands.json，供代码补全和跳转。
- settings.json：调试工具目录 scGcd.toolchainBin，保留已有 CMake Tools 设置和错误提示；补充格式规则。
- extensions.json：保留现有 C/C++、CMake Tools 推荐。

默认工具目录 C:/Strawberry/c/bin，可在 settings.json 调整 scGcd.toolchainBin。
既有 CMake Tools 配置的 compilerPath 和 configureSettings 是本机原设置，换工具目录时也应同步修改。
Debug 使用 build/debug，不覆盖原 build 下的 Release 程序。首次构建后代码补全数据库才存在。
若同时使用 CMake Tools 工具栏，原 CMake Tools 仍指向 build；本文 F5/运行任务统一走 build/debug。

## 输出与断点

Stage1 结果：build/debug/stage1-output.txt；统计：stage1-stats.csv；事件：stage1-events.csv。
Day01 波形：build/debug/day01_basics.vcd。启动参数、目录及运行库 PATH 均已配置。
Parser 有 EOF 后仍被时钟唤醒并立即返回的情况，可在断点条件设置 `!m_eof`，不必改变模型代码。
不要把 SC_THREAD 当作独立操作系统线程；内部 wait/内核调度可能带来跨栈跳转。初学时优先在自己的 tick 内断点，不逐行进入整个内核。

## 已验证范围

- JSON 均可解析；构建任务对应脚本在 Debug 模式实际成功；Release 与 Debug 均为 2/2 CTest 通过。
- GDB 14.2 实际命中 Parser::tick，读取 m_sent=0，执行 next 后继续，8 条任务/79 周期正常退出。
- GDB 实际命中 Testbench::run，读取 m_testFailures=0，单步后继续并 PASS。
- 未操作 VS Code 图形界面点击 F5；以上证明配置使用的程序、符号、调试器及参数可用，不宣称 GUI 全流程已经实测。

官方配置依据：https://code.visualstudio.com/docs/cpp/launch-json-reference 、https://code.visualstudio.com/docs/cpp/config-mingw 。
