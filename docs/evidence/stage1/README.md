# 作业 1 验证证据

日期：2026-09-06。代码基线 fdd3ecb 加本条所在提交的作业 1 实现和修正。
执行位置：项目根目录；命令 `./scripts/build-test.ps1`；Windows GCC 13.2 Release、SystemC 3.0.1、Python 3.14.4。

- initial-test-failure.log：第一次 CTest 原始失败日志；手写预期误将 gcd(1024,17) 写成 4。保留原始文件，时间行可能包含本地编码文字。
- passing-tests.log：最终 2/2 CTest 通过日志；8 个成功场景累计处理 562 条任务，另检查非法输入及超时。
- basic.output.txt / basic.stats.csv：基本八组任务的功能和性能输出。
- single.events.csv：手工规定的单任务 12 周期时间线对应的实际事件。

运行 verify.py 会在 build/stage1-tests 生成每个场景的输入、输出、事件与统计；随机种子固定为 20260906。
初次失败到最终通过的版本均在同一工作树完成，未编造中间历史提交；具体定位过程见 B-003。
