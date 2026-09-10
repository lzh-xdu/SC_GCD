"""Lightweight SystemC structural parser (stdlib only).

Targets the project's coding style:
  - ``SC_MODULE(Name) { ... }`` or ``class Name : public sc_module`` blocks
  - ports: ``sc_core::sc_in/out/inout/fifo_in/fifo_out/fifo/signal<...> name{"label"};``
  - channels: ``sc_core::sc_clock name;`` and ``sc_core::sc_fifo<T> name;``
  - bindings: ``instance.port(member);`` inside the top module constructor

This is a pragmatic regex parser, not a C++ front end. It reads only the
structure it needs; expressions that do not follow the single-identifier
binding form (e.g. TLM sockets, ``sc_port``, positional binding) are left for a
future libclang-based parser.
"""

import re

from .model import Graph, Module, Port, Wire

_MODULE_RE = re.compile(r"\bSC_MODULE\s*\(\s*([A-Za-z_]\w*)\s*\)")
_SUBCLASS_RE = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)\s*:\s*public\s+sc_module\b")

# kind, optional element type, member name, optional {"label"}
_MEMBER_RE = re.compile(
    r"sc_core::(sc_in|sc_out|sc_inout|sc_fifo_in|sc_fifo_out|sc_fifo|sc_signal|sc_clock)"
    r"\s*(?:<([^>]*)>)?\s+([A-Za-z_]\w*)\s*"
    r'(?:\{\s*"([^"]*)"\s*\})?\s*;'
)

# instance.port(member);
_BINDING_RE = re.compile(r"\b([A-Za-z_]\w*)\.(\w+)\s*\(\s*([A-Za-z_]\w*)\s*\)\s*;")

_PROCESS_RE = re.compile(r"\bSC_(METHOD|THREAD|CTHREAD)\s*\(\s*(\w+)\s*\)")

_EVENT_RE = re.compile(r'\brecord\s*\([^,]*,\s*"([a-z_]+)"')

_KIND_DIRECTION = {
    "sc_in": "in",
    "sc_out": "out",
    "sc_inout": "inout",
    "sc_fifo_in": "in",
    "sc_fifo_out": "out",
    "sc_fifo": "internal",
    "sc_signal": "internal",
    "sc_clock": "clock",
}

_PORT_KINDS = ("sc_in", "sc_out", "sc_inout", "sc_fifo_in", "sc_fifo_out")


def _mask_comments(text):
    """Replace // and /* */ comments with spaces, preserving line structure."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            out.append(" ")
            out.append(" ")
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append(" ")
            out.append(" ")
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _matching_brace(text, open_index):
    """Return the index just after the brace that closes text[open_index]."""
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError("unbalanced braces in source")


def _find_blocks(text):
    """Yield (name, block_text) for every module definition in text."""
    for match in _MODULE_RE.finditer(text):
        open_index = text.find("{", match.end())
        if open_index < 0:
            continue
        close_index = _matching_brace(text, open_index)
        yield match.group(1), text[open_index + 1:close_index - 1]
    for match in _SUBCLASS_RE.finditer(text):
        open_index = text.find("{", match.end())
        if open_index < 0:
            continue
        close_index = _matching_brace(text, open_index)
        yield match.group(1), text[open_index + 1:close_index - 1]


def _topo_order(instance_names, fifo_pairs):
    """Return instance names ordered left-to-right along the dataflow graph.

    ``fifo_pairs`` are (source, target) instance pairs. Remaining nodes in a
    cycle keep their input order.
    """
    from collections import deque

    indegree = {name: 0 for name in instance_names}
    adjacent = {name: [] for name in instance_names}
    for source, target in fifo_pairs:
        if source in indegree and target in indegree:
            adjacent[source].append(target)
            indegree[target] += 1
    queue = deque(sorted(name for name, degree in indegree.items() if degree == 0))
    result = []
    while queue:
        node = queue.popleft()
        result.append(node)
        for neighbour in sorted(adjacent[node]):
            indegree[neighbour] -= 1
            if indegree[neighbour] == 0:
                queue.append(neighbour)
    result += [name for name in instance_names if name not in result]
    return result


def _parse_ports(block):
    """Return (ports, process_names) found in a module block."""
    ports = []
    processes = []
    for match in _MEMBER_RE.finditer(block):
        kind, element, name, label = match.group(1), match.group(2) or "", match.group(3), match.group(4) or ""
        ports.append(Port(name=name, direction=_KIND_DIRECTION.get(kind, "internal"),
                          kind=kind, element=element, label=label))
    for match in _PROCESS_RE.finditer(block):
        processes.append(match.group(2))
    return ports, processes


def parse(files, top=None, clock_period_ns=1.0):
    """Parse SystemC sources into a :class:`Graph`.

    ``files`` may be header and implementation files; they are concatenated
    after stripping comments. ``top`` selects the top module (default: auto-detect,
    the module that instantiates other modules and holds the sc_clock/sc_fifo channels).
    """
    text = _mask_comments("\n".join(open(path, encoding="utf-8-sig").read() for path in files))

    blocks = list(_find_blocks(text))
    if not blocks:
        raise ValueError("no SC_MODULE / sc_module module found in input files")

    # module type -> block text (first definition wins)
    blocks_by_type = {}
    for name, block in blocks:
        blocks_by_type.setdefault(name, block)
    module_types = set(blocks_by_type)

    # ports per module type
    ports_by_type = {}
    processes_by_type = {}
    for name, block in blocks_by_type.items():
        ports, processes = _parse_ports(block)
        ports_by_type[name] = ports
        processes_by_type[name] = processes

    # pick top module: explicit, or the module instantiatiating other modules
    if not top:
        candidates = []
        for name, block in blocks_by_type.items():
            instances = [t for t in module_types if t != name and re.search(r"\b" + t + r"\s+([A-Za-z_]\w*)\s*;", block)]
            if instances:
                candidates.append((len(instances), name))
        top = max(candidates)[1] if candidates else list(module_types)[-1]

    top_block = blocks_by_type[top]

    clock_match = re.search(r"sc_core::sc_clock\s+([A-Za-z_]\w*)\s*;", top_block)
    clock_name = clock_match.group(1) if clock_match else ""

    fifo_names = {m.group(1) for m in re.finditer(r"sc_core::sc_fifo<[^>]*>\s+([A-Za-z_]\w*)\s*;", top_block)}

    # instance name -> type
    instances = {}
    for tpe in sorted(module_types):
        if tpe == top:
            continue
        for match in re.finditer(r"\b" + re.escape(tpe) + r"\s+([A-Za-z_]\w*)\s*;", top_block):
            instances.setdefault(match.group(1), tpe)

    # bindings: instance.port(member)
    bindings = []
    for match in _BINDING_RE.finditer(text):
        instance_name, port_name, member = match.group(1), match.group(2), match.group(3)
        if instance_name not in instances:
            continue
        bindings.append((instance_name, port_name, member))

    # module port direction lookup
    def port_dir(instance_name, port_name):
        tpe = instances[instance_name]
        for port in ports_by_type.get(tpe, []):
            if port.name == port_name:
                return port.direction
        return ""

    graph = Graph(top=top, clock=clock_name, clock_period_ns=clock_period_ns, files=list(files))

    # clock wires
    for instance_name, port_name, member in bindings:
        if member == clock_name:
            graph.wires.append(Wire(source=clock_name, target=instance_name, kind="clock",
                                    source_port="", target_port=port_name, label=port_name))

    # fifo wires: group bindings per fifo member, then pair out -> in
    fifo_bindings = {}
    for instance_name, port_name, member in bindings:
        if member in fifo_names:
            fifo_bindings.setdefault(member, []).append((instance_name, port_name))

    for fifo, port_bindings in fifo_bindings.items():
        sources = [pb for pb in port_bindings if port_dir(pb[0], pb[1]) in ("out", "inout")]
        targets = [pb for pb in port_bindings if port_dir(pb[0], pb[1]) == "in"]
        for src_instance, src_port in sources:
            for dst_instance, dst_port in targets:
                if src_instance != dst_instance:
                    graph.wires.append(Wire(source=src_instance, target=dst_instance, kind="fifo",
                                            source_port=src_port, target_port=dst_port, label=fifo))

    # event name -> module type, by longest lowercased-type prefix of the event name
    all_events = sorted(set(_EVENT_RE.findall(text)))
    events_by_type = {}
    for event in all_events:
        best = None
        for tpe in sorted(module_types - {top}, key=len, reverse=True):
            low = tpe.lower()
            if event.startswith(low) and (best is None or len(low) > len(best[0].lower())):
                best = tpe
        if best:
            events_by_type.setdefault(best, []).append(event)

    # topologically order instances along the dataflow (fifo) wires
    fifo_pairs = [(w.source, w.target) for w in graph.wires if w.kind == "fifo"]
    order = _topo_order(list(instances), fifo_pairs)

    # build graph modules: clock node + one node per instance
    if clock_name:
        graph.modules.append(Module(id=clock_name, type="Clock"))
    for instance_name in order:
        tpe = instances[instance_name]
        module = Module(id=instance_name, type=tpe)
        for port in ports_by_type.get(tpe, []):
            if port.kind in _PORT_KINDS or port.kind == "sc_clock":
                module.ports.append(port)
        module.events = events_by_type.get(tpe, [])
        graph.modules.append(module)

    return graph