# SystemC Web Viewer（模块结构与周期动画）

将从 SystemC 源码解析出的模块/端口/连接结构，与真实仿真事件 CSV 结合，生成**自包含单文件 HTML**，在浏览器中动画展示模块链接与运行周期。零第三方依赖、无 CDN，双击即可打开。

## 输入与输出

| 输入 | 说明 |
|---|---|
| `--src` 若干 `.hpp/.cpp` | 从 `SC_MODULE` 声明、端口成员、构造函数绑定中解析结构 |
| `--events` 一个 CSV | 仿真事件 trace（`id,event,cycle,a,b,value,latency`），与 `trace_tui.py` 同源 |
| `--out` 一个 `.html` | 生成的自包含页面，内嵌全部数据 |

## 用法（作业 1 示例）

```powershell
python -B scripts/scviz.py `
  --src src/stage1/main.cpp src/stage1/parser.hpp src/stage1/parser.cpp `
         src/stage1/transform.hpp src/stage1/transform.cpp `
         src/stage1/compute.hpp src/stage1/compute.cpp `
         src/stage1/output.hpp src/stage1/output.cpp `
  --events build/debug/stage1-events.csv `
  --top System --clock-period 1.0 `
  --out web/stage1.html
```

然后直接打开 `web/stage1.html`，或 `python -m http.server -d web` 后访问。

## 页面内容

1. **模块与连接结构（上半 SVG）**：Clock 节点 + `Parser → Transform → Compute → Output` 四个模块（按 FIFO 数据流自动拓扑排序），FIFO 连线标注通道名，时钟虚线扇形扇出到各模块 `m_clk` 端口。
2. **仿真周期（下半 SVG）**：每条任务一行，按 `F/T/Q/C/B/O` 区间着色（与 `trace_tui.py` 语义一致），`*` 标记输出完成沿；顶部为周期轴与播放头。
3. **播放控制条**：播放/暂停、单步、起止、速度、拖动播放头、键盘 `Space`/`←`/`→`。

动画联动：播放头所在周期，发生事件的模块高亮，输出型事件（`…_emit`/`…_send`/`output`）使其 FIFO 出线脉冲，时钟节点每周期闪烁；当前周期事件列表显示在顶部。

## 实现结构

```
scripts/scviz.py           CLI 入口
scripts/scviz/sc_parser.py SystemC 结构解析（正则 + 括号匹配）
scripts/scviz/events.py   事件 CSV 载入 + 区间计算 + 事件→模块映射
scripts/scviz/webgen.py   数据组合 + 注入 template.html
scripts/scviz/template.html 前端（原生 JS + SVG，无 CDN）
scripts/scviz/model.py    数据模型 dataclass
```

## 数据来源与真实性

- **结构**：正则解析 `SC_MODULE`/`class X : public sc_module`、`sc_in/out/fifo_in/fifo_out/signal/clock` 成员、`instance.port(member)` 绑定、`record(…, "event")` 字符串字面量。事件名按"模块类型小写为前缀"自动归属到模块。
- **周期**：直接使用真实仿真事件 CSV，不另做伪仿真器；区间 `[start, stop)` 与 `trace_tui.py` 完全一致，动画展示的是实际仿真时序而非推测。

## 局限与后续

- 正则解析非 C++ 前端：只识别单标识符绑定 `a.b(x)`，不识别 TLM socket、`sc_port`、位置绑定、多重绑定 `bind()`。
- 事件→模块靠名字前缀自动匹配，异构命名需手动映射（后续加 `--event-map`）。
- 同类模块多实例时事件只能归属到第一个实例。
- 布局为单行拓扑，复杂层级（嵌套子模块递归展开）与内部 `sc_signal` 节点暂未逐层展开。
- 后续可选：libclang 精确解析、VCD 波形接入（已有 `day01_basics.vcd`）、TLM-2.0 socket 图。

设计取舍与协作记录见 [decisions.md](decisions.md) / [ai-log.md](ai-log.md)。