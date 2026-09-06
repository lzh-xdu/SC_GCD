# 作业 3 验证证据

2026-09-06。基线为作业2提交75c76f5，加本目录所在提交的stage3实现、比较脚本和测试扩展。

- capacity-before：主动探测时R=1的实际input/output/stats/events；48任务、D=2、输出周期3，总663周期/结果等待74周期。
- capacity-after：同实现显式R=2，总648周期/等待7周期；随后默认值由1改为2。
- capacity-larger：同实现R=4，总648周期/等待0周期，说明继续加缓冲在此输入未再缩短总周期。
- 上述探测发生在未提交的作业3首版，未故意注入功能错误；最终Collector又将id检查置于输出写入前，完整回归结果不变。B-006记录的是真实容量设计缺陷与配置改进，不声称存在GCD错误。
- test-edit-failure.log：AI编辑Python时误缩进for造成测试无法启动；与容量案例无关，见B-007。
- debug-tests.log：最终Debug CTest 5/5。
- release-tests.log / release-stage1-regression.log：Release其余4项及独立输出目录的作业1回归通过。
- stage3-summary.csv / comparison.csv：最终模拟统计与单/双比较；Release/Debug内容一致。
- 各验证场景子目录保存最终实际output/stats；输入可由tests/handshake/verify.py及tests/stage1/cases、tests/stage3/cases重建。对比原始文件在comparison子目录。

复现命令见stage3-run.md；事件文件较大的正常场景保留在build/debug/stage3-tests，主动案例事件纳入证据。所有数值都是模拟周期/资源代理，非硅面积功耗或主机效率测量。
