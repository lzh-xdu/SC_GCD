# Model 分层重构验证

2026-09-09；基线 `0df1381`，改动与本记录同提交。Windows、Strawberry GCC、SystemC 3.0.1；使用既有 Release/Debug 构建目录。

## 实际结果

| 检查 | 实际结果 |
|---|---|
| 重构前 `ctest --test-dir build --output-on-failure` | 12/12，退出 0，5.00 秒 |
| 重构后 `cmake --build build --parallel 4` | 修正下述字段迁移遗漏后退出 0 |
| 重构后 Release CTest | 12/12，退出 0，7.30 秒；已增加成对运行 |
| `cmake --build build/debug --parallel 4` | 退出 0 |
| 重构后 Debug CTest | 12/12，退出 0，7.56 秒；已增加成对运行 |
| 重构前后文件 SHA-256 对比 | 保存的 372 份 output.txt、stats.csv、events.csv 全部相同 |
| Trace 开/关逐字节对比 | 作业 1：24；作业 2：19；作业 3 轮转：22；作业 3 窗口：31；两种构建各 96 个有效场景通过 |
| 头文件格式 | 所有项目头文件均含 @brief、Interface、Protocol、Timing、Reset |

372 份文件来自四个阶段回归目录，包含既有异常场景留下的文件；不等同于 372 个有效测试场景。
基线 SHA-256 清单保存在本地 `tmp/model-refactor/baseline.json`，临时目录不提交。
常规回归仍检查独立数值参考、事件时序、FIFO 容量、握手稳定和保序；成对检查新增调用见 `tests/instrumentation_equivalence.py`。
最终仅调整了注释图框和文档，没有在验证后改变功能实现。

## 真实构建错误与修正

触发：将 Compute 计数移入 m_statistics 后首次执行 Release 构建，退出 1。
原始诊断摘录（stage3/main.cpp，当时行 279）：

```text
error: 'const struct stage2::Compute' has no member named 'm_busyCycles'
stats << "compute" << unit << "_busy_cycles," << computeUnits[unit]->m_busyCycles
error: 'const struct stage2::Compute' has no member named 'm_accepted'; did you mean 'accept'?
error: 'const struct stage2::Compute' has no member named 'm_resultWaitCycles'
ninja: build stopped: subcommand failed.
```

原因：迁移时更新了对象成员访问，漏掉双 Compute 报告中的指针访问；检查还发现窗口占用观察中的本地派发计数访问需同步。
修正：两个阶段的 `computeUnits[unit]->m_statistics.*` 及窗口占用观察同步更新。随后两种构建和上述回归通过。
这是本轮 AI 重构产生并修正的编译错误，不冒充作业 3 的硬件设计缺陷案例。

## Profiling 烟雾验证

执行：

```powershell
python scripts/profile_model.py --report tmp/model-refactor/profile.json --repeat 3 -- build/stage1_gcd.exe tests/stage1/basic.txt tmp/model-refactor/profile-output.txt tmp/model-refactor/profile-stats.csv 2 -
```

退出 0；3 次结果和统计 SHA-256 相同。主机进程耗时依次为 0.0282559、0.0207347、0.0208132 秒；中位数 0.0208132 秒。
此数据仅证明测量路径可用，包含进程启动和 I/O，不用于宣称重构提速。
未提供全部 Instrumentation 总开关；本次成对证据仅针对现有 Trace 开关，Statistics 保持开启。
