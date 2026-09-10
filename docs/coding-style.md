# 项目代码规范

依据用户指定的 D:/code/0_qt_github/codex/ohos-qt-kb/semantic/cpp-cleancode-rules.md（2026-09-06 版本）执行。
本地路径为规范来源，不要求其他机器存在该路径。

- 类/结构体 PascalCase；成员 m_ + camelCase；函数 camelCase；常量和枚举值 UPPER_SNAKE_CASE。
- 测试专用局部变量/参数 test 前缀；测试成员 m_test 前缀；测试常量 TEST_ 前缀。测试观测日志使用 TestEventLog。
- 硬件任务、队列、寄存器、功能输出和性能统计仍按业务命名，不因被测试就全部增加 test。
- Python 黑盒测试中的自定义变量同样使用 test_ 前缀；函数可用测试脚本的 snake_case。
- 每行不超过 120 字符；函数不超过 50 行；头文件内联函数不超过 10 行；嵌套不超过 4。
- 所有控制语句加大括号；一行一个声明；初始化项逐行、前导逗号；命名类型转换；显式 lambda 捕获。
- 文件头使用 Copyright (c) 2026 SC_GCD contributors. All rights reserved.，描述项目贡献者，不冒用雇主版权；未引入第三方许可变更。
- 保留设计解释注释，自己的头文件优先，其后第三方和标准库。
- 使用 C++17，因为当前 SystemC 3.0.1 依赖它；规范所述 C++11 是规则兼容基线，不降级项目语言标准。
- sc_main、SC_MODULE、SC_METHOD 等 SystemC 约定名称/API 保留，不强行改名；库代码不纳入项目风格重写。
- 本项目无 GUI；控制台错误、诊断、CSV 标签作为日志/机器接口保持原字符串，不添加无实际需求的翻译框架。
- .clang-format 处理排版；函数长度、命名、语义常量与测试职责需人工复核，格式工具不保证全部规则。

变更须通过现有功能/时序回归，保持数值和处理周期不变。

## 2026-09-07：命名和运行时契约补充

- 配置常量名称表达对象、用途和单位；命令行位置用 ARGUMENT_INDEX，容量用 CAPACITY_TASKS，周期用 CYCLES。
- 函数入口检查真实前置条件；操作完成后检查 I/O 结果，数据取得后检查业务约束。使用 common/contract.hpp 的 assertCondition，Release 同样抛异常。
- 默认 runtime_error；非法参数可选 invalid_argument，内部不变量可选 logic_error。保留有诊断价值的错误说明。
- 不重定义标准 assert；不在可被 NDEBUG 消除的断言中执行 nb_read/nb_write 等业务操作。
- 纯查询/全域合法计算无需虚构断言；头文件记录参数单位、借用寿命、调用约束和异常，保留时序说明。
- 实施范围、例外和验证见 [命名与运行时契约](archive/code-contracts.md)。

## 2026-09-09：头文件接口注释

- 按用户指定格式，模块/头文件的主要说明使用 Doxygen 块，依次包含 `@brief`、`Interface:`（ASCII 图）、`Protocol:`、`Timing:`、`Reset:`。
- 接口图使用真实端口或函数接口；协议、时序和初始化必须与实现一致。无复位端口须明确说明，不照搬示例中的低有效同步复位。
- 数据类型、无状态工具和被动统计也使用上述结构，注明不适用的硬件时序/复位语义；成员函数的简短契约保留为 Doxygen 块。
- spec 外的边界约定显式标记 Model Assumption；保留参数寿命、异常、背压和统计职责说明。

## 2026-09-09：Model 分层补充

- 前述 TestEventLog 命名被 EventRecorder 替代：该观察器同时承载正式任务统计，成员引用为 m_recorder；纯测试流仍为 m_testStream。
- 模块观测计数统一归入 m_statistics，硬件控制状态保留在模块中；共享数值运算和延迟规则见 [Model 分层](archive/model-structure.md)。
- clang-format 关闭 ReflowComments，避免自动重排破坏头文件 ASCII 接口图。
