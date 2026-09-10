# B-011：用 perf 定位并优化完整 Trace

2026-09-11。用户提出用 perf 对“仿真程序生成 Trace 的过程”采样，再依据热点优化。此处是**主机用户态 CPU 采样**；仿真 CSV 仍为完整打点日志，未改成抽样或省略事件。

提交的工具文本报告仅清理行末空格；数值、错误信息和样本未改写。

**09-11 后续方法纠正：**用户指出本轮只优化了作业 4，表中的时钟模型仍使用旧日志编码。因此 3.61× 只证明作业 4 自身的 Trace 优化收益，不能将“新作业 4 / 旧作业 3”的差距归因于事件调度。作业 3 同等优化后的四组对照见 [公平对照报告](../fair-trace/README.md)；本文件保留原始实验，不替换历史样本。

## 1. 基线与测量方法

- 源码基线 `b2207d3`，工作区初始干净；优化源码及新增测试为本条所在提交。基线二进制在修改前另存。CPU、内核、编译器与三个二进制 SHA256 见 [environment.json](environment.json)（环境文件在测量后补记，明确标注）。
- WSL2 / i5-12400F / GCC 11.4 / SystemC 3.0.1；两版均 `-O3 -g -DNDEBUG -fno-omit-frame-pointer`。本次数字不是旧 Windows 入口径，也不直接拼接旧报告。
- Ubuntu 官方 `linux-tools-5.15.0-136_5.15.0-136.147_amd64.deb`，解包使用 perf 5.15.178。软件 `cpu-clock:u` 在当前 WSL 内核实测可用；没有修改 perf 权限或系统全局配置。包 SHA512 与 apt 元数据一致：`c29437fa2c85776613442d0d837d156d65ff4bf46bd2fb6ed6954e4f4b690647324f8bab376c6bc0fde3f389749726fb55e975ba3980fbed3dcddb3e19a9a613`。
- perf：997 Hz、帧指针调用栈；同一万项混合输入，优化前重复 12 次取得 **3391 样本**，优化后重复 36 次取得 **2973 样本**，均无丢样。增加优化后重复次数是为获得足够样本；不以两份采样总时间计算加速比。
- 系统库未统一保留帧指针，inclusive 栈中有未知/截断帧，因此优先解释 **Self 自身 CPU 样本占比**；Children 包含子调用，不能相加。perf 不直接解释等待 I/O 的墙钟时间。用法依据 [perf record 手册](https://www.man7.org/linux/man-pages/man1/perf-record.1.html)。
- 正式耗时在**不运行 perf、也不并行构建**时另测：四场景 × 三程序 × 七次＝84 次，另各预热一次；每轮正反交替程序顺序。`perf_counter` 测整个进程墙钟，`wait4` 测用户/系统 CPU 与峰值 RSS；输入和输出在 Linux 原生临时目录，避免 Windows 挂载目录写入差异。该进程 RSS 口径受进程启动/父进程足迹下限影响，不等于日志堆分配量或硬件资源。

## 2. 观察与定位

触发思路沿用 B-011：事件模型虽然跳过无变化周期，详细日志可能重新逐周期做工作。主动打开 Trace，检查性能退化究竟落在哪里，而非仅凭函数名称推断。

[优化前 Self](before/self.txt) 中主要热点包括：

| 函数/路径 | Self CPU 占比 | 解释边界 |
|---|---:|---|
| `__merge_sort_with_buffer` | 6.58% | Trace 全量稳定排序 |
| `__move_merge` | 6.46% | Trace 归并移动 |
| `basic_streambuf::xsputn` | 7.11% | 流缓冲复制；不全等于文件写入 |
| `ostream::_M_insert<unsigned long>` | 5.90% | 数字流格式化 |
| `__ostream_insert` | 5.63% | 字符串流插入 |
| `num_put::_M_insert_int<unsigned long>` | 4.19% | 数字格式化底层 |

结合源码：`append` 每行建立 ostringstream、生成字符串并缓存；`repeat` 为跳过区间逐周期建立行；`flushTrace` 最后稳定排序全部字符串再逐行写出。前两项可直接归到 Trace 排序；其他标准库函数也可能被输入/输出复用，不能把其全部比例武断归到 Trace，更不能将 inclusive 百分比累计。

## 3. 实际改进

1. 保存结构化事件和连续周期区间，拥有事件名字符串，避免 `string_view` 在延迟输出时悬空；重复区间不在采集时展开。
2. 在 System 已记录当前边界及整个跳过区间后、进入下一个仿真时刻前调用 `flushBatch`。此时不会再出现更早的事件，因此可以安全输出本批。
3. 对本批区间做小堆多路合并，按周期排序，同周期按原插入序号打破平局，保持旧 `stable_sort` 的结果；区间逐行展开只发生在最终 CSV 编码时。
4. `to_chars` 转十进制数字、复用字节缓冲，达到 64 KiB 写出阈值后批量写入；结束时写出尾部。阈值不是精确最大容量（字符串容量增长、单行长度需另计）。

没有改变调度期限、FIFO 可见时序、任务统计口径或日志字段。正式模型每批回收事件区间，Trace 存储不再随整个运行的日志行数累计；统计模块保存任务延迟样本的开销仍存在。最终仍需输出所有 CSV 行，时间不可能与日志大小无关。

## 4. 改进前后结果

墙钟中位数；完整样本见 [samples.json](comparison/samples.json)，汇总见 [summary.json](comparison/summary.json)，四分位与哈希核查见 [validation.json](comparison/validation.json)。

| 场景 | 原事件模型 ms | 改进事件模型 ms | 时钟模型 ms | 相对原事件模型 |
|---|---:|---:|---:|---:|
| 混合一万项，Trace on | 302.77 | 83.83 | 221.25 | 3.61× |
| 同混合输入，Trace off | 34.92 | 35.30 | 93.08 | 基本持平 |
| 零延迟一万项，Trace on | 70.47 | 27.66 | 41.71 | 2.55× |
| 100 短任务、输出间隔 1000，Trace on | 128.53 | 22.94 | 95.88 | 5.60× |

- 混合 Trace on：墙钟 Q1～Q3 为 **296.14～333.05 ms → 80.61～91.57 ms**；用户 CPU 中位数 **273.96 → 70.58 ms**；峰值 RSS **73980 → 21052 KiB（72.25 → 20.56 MiB）**。
- 混合 Trace off：中位数约 +1.1%，两者四分位范围重叠，不宣称严格零开销。
- 混合 Trace 均为 **21635542 字节**，三程序的结果、全部统计、完整 Trace 的 SHA256 一致；每次运行还用独立 `math.gcd` 逐项检查输出。其他两组 Trace 也逐字节等价。混合 Trace on/off 的结果和统计哈希另行核对一致。
- [优化后 Self](after/self.txt)：旧 Trace 全量排序热点消失；现在 `appendNumber` 18.23%、字符串 `_M_replace` 11.03%、`flushBatch` 6.02%。这是更小总 CPU 工作量中的占比，不表示数字处理变慢；仍有进一步优化空间，本轮不宣称最优。

## 5. 回归及真实过程问题

- 新增 `stage4_trace_ordering`：独立稳定排序参考，覆盖交错/重叠区间、同周期顺序、零长度区间、临时事件名寿命、uint64 最大值、跨字节缓冲与批次、重复 drain、写流失败。
- Linux 优化构建 [26/26](all-tests.log)、Linux Debug [26/26](debug-tests.log)、Windows Release [26/26](windows-tests.log)、Windows Debug [26/26](windows-debug-tests.log)，构建均无编译警告。原阶段的数值、时序、背压、等价检查均保留；最终复现脚本另经语法检查及 [单轮冒烟](script-smoke.log)，不混入正式七轮测量。
- 首次 DWARF 采样报告 `failed to write perf data, error: Bad address`，见 [原始错误](initial-record-failure.log)；后一次临时目录已不存在，见 [重试错误](record-before.log)。未证明具体环境根因；采用原生临时目录、较小单次负载、帧指针模式后成功，不将该错误归为模型缺陷。
- 新目标尚未生成导致构建找不到目标（[原始记录](initial-build-target-failure.log)）；重新配置后，新增测试漏包含 SystemC 头导致 `sc_main` 链接失败（[原始记录](initial-test-link-failure.log)），补充头文件修复；首次筛选 Stage4 测试时入口检查程序尚未构建（[原始记录](initial-unbuilt-tests.log)），补全目标后全量通过。均为 AI 构建/测试组织问题，不作为 B-011 本身。

## 6. 复现

先在基线 `b2207d3` 的独立源码目录按同样编译参数构建，保存其 `stage4_gcd` 和 `stage3_window_gcd`，再构建本条提交；不要将当前程序复制成“优化前”。依赖 SystemC 源码按项目原流程准备。命令中 `BEFORE`、`CLOCK`、`PERF` 分别替换为保存的旧事件程序、时钟程序和可用 perf 路径。

```bash
cmake -S . -B build-linux/perf-trace -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O3 -g -DNDEBUG -fno-omit-frame-pointer"
cmake --build build-linux/perf-trace -j8
ctest --test-dir build-linux/perf-trace --output-on-failure
python3 scripts/profile_trace.py --before BEFORE --perf PERF --repeat 12 \
  --report tmp/perf-trace/reproduce-before
python3 scripts/profile_trace.py --before build-linux/perf-trace/stage4_gcd --perf PERF \
  --repeat 36 --report tmp/perf-trace/reproduce-after
python3 scripts/profile_trace.py --before BEFORE --after build-linux/perf-trace/stage4_gcd \
  --clock CLOCK --repeat 7 --report tmp/perf-trace/reproduce-comparison
```

核心采样命令为 `perf record -e cpu-clock:u -F 997 --call-graph fp -- PROGRAM ...`；Self 报告使用 `perf report --stdio --no-children --call-graph none`。脚本保留报告，原始 `.data` 只保存在被忽略的项目 `tmp/perf-trace/`，不提交工具包、二进制或大日志。WSL `perf` 默认包装器可能按内核版本寻找不存在的路径，本次直接调用解包后的 perf 可执行文件。

结论范围：B-011 的 **Trace 观测实现缺陷**已有实际改进及回归闭环；仍不将它改称为被模拟硬件或事件调度功能错误。
