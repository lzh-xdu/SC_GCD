# 作业 1 Transform 正向重构验证

2026-09-09；基线 `2953df6`，源码变更为本条所在提交的 `src/stage1/transform.cpp` 与接口注释。用户已有暂存 README 修改不属于本次工作单元。

## 命令与实际结果

Windows PowerShell，PATH 前置 `C:\Strawberry\c\bin`，复用项目 SystemC 3.0.1 与既有构建配置。

| 检查 | 命令/方法 | 实际结果 |
|---|---|---|
| 修改前 Release | `ctest --test-dir build -R '^stage1_function_timing_backpressure$' --output-on-failure` | 1/1，退出 0，1.68 秒 |
| 修改后 Release 构建 | `cmake --build build --target stage1_gcd --parallel 4` | 退出 0 |
| 修改后 Release 回归 | 同上 CTest 命令 | 1/1，退出 0，1.51 秒 |
| 修改后 Debug 构建 | `cmake --build build/debug --target stage1_gcd --parallel 4` | 退出 0 |
| 修改后 Debug 回归 | `ctest --test-dir build/debug -R '^stage1_function_timing_backpressure$' --output-on-failure` | 1/1，退出 0，1.69 秒 |
| 重构前后逐文件等价 | Get-FileHash SHA256，对比 output.txt、stats.csv、events.csv | 102 份全部相同 |

两种构建各包含 24 个有效场景、1434 项任务、14 类非法输入及 Trace 开关成对校验。流水场景检查接收沿 2..21、输出沿 4..23；深度 1、慢输出、随机与数值边界覆盖阻塞及恢复。102 份文件包括非法输入场景的部分产物，不表示 102 个有效场景。基线清单位于本地临时文件 `tmp/stage1-forward-baseline.json`，不提交。

预期为功能、事件顺序、周期与统计全部保持；实际与预期一致。当前状态只在末尾赋值，排序/输出读取旧槽；ready 允许排空与补位同拍发生。实际 sc_fifo 源码检查确认 num_free 与 nb_write 使用相同空位条件，SC_METHOD 内无 wait。未新增测试脚本，未重跑未修改的作业 2/3，也未测主机运行性能。

## 2026-09-09：用户选择恢复倒序

- 当前轮基线 `2010035`；用户明确选择倒序后恢复原地更新，仅补充背压和同拍补位注释，同步头文件。变更与本条同提交，原正向验证保持为历史事实。
- 复用上表命令，Release/Debug 的 stage1_gcd 构建均退出 0，阶段 1 CTest 各 1/1 通过（每种 24 个有效场景/1434 项任务、14 类非法输入及 Trace 开关校验）。
- Release 的 102 份输出、事件和统计文件与原 `2953df6` SHA256 清单全部相同；该清单亦已验证与正向实现相同。clang-format 的 cpp 检查通过。
- 未运行其他阶段或主机性能基准。恢复是用户取舍，不是因正向方案出现功能失败；参见 D-019。
