# 作业 4 初始整合验证

2026-09-09；旧源码基线 `a52e13c1b47641a818836ef5982ca4af698fe999`；新源码、CMake 与测试为本条所在提交。用户原有暂存 README 标题修改不参与模型，也不纳入本次提交。

| 命令 | 结果 |
|---|---|
| `cmake --build build --parallel 4` | Release 构建成功，退出 0 |
| `cmake --build build/debug --parallel 4` | Debug 构建成功，退出 0 |
| `ctest --test-dir build --output-on-failure` | 13/13，退出 0 |
| `ctest --test-dir build/debug --output-on-failure` | 13/13，退出 0 |

新增 `stage4_initial_equivalence` 调用 `tests/stage4/compare_initial.py`，分别运行既有独立数值/边沿/握手/容量检查，然后比较生成文件集合及内容。两种配置各 276 份文件一致；覆盖范围见 [初始版本](../../stage4-initial.md)。失败会保留生成文件供定位；本轮没有构建或测试失败。

原始测试摘要：[Release](release-tests.log)、[Debug](debug-tests.log)；等价断言输出见 [comparison.txt](comparison.txt)。完整运行产物在被忽略的 `build/stage4-initial-tests/` 与 `build/debug/stage4-initial-tests/`，可用上述命令重建。

源码隔离检查：新目标 15 个 cpp 全位于 `src/stage4/`；所有项目 include 解析后仍位于该目录；旧阶段与旧共享源码 Git 差异为空。此次验证证明初始整合等价，不证明未来事件模型等价或更快。
