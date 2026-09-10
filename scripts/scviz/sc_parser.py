"""Lightweight SystemC structural parser (stdlib only).

Targets the project's coding style:
  - ``SC_MODULE(Name) { ... }`` or ``class Name : public sc_module`` blocks
  - scalar ports/channels: ``sc_core::sc_in/out/inout/fifo_in/fifo_out/fifo/signal/clock<...> name{"label"};``
  - vector ports/channels: ``sc_core::sc_vector<sc_core::sc_...<...>> name{"label", COUNT};``
  - bindings: ``instance.port(member);`` plus the loop/indexed idioms used for the
    compute fan-out (``array[i]->port(ch[i])``, ``obj.port[i](*array[i])``).

This is a pragmatic regex parser, not a C++ front end. It normalises vector
members and loop bindings into scalar statements first, then pairs signal/FIFO
channel bindings into source -> target wires. TLM sockets, ``sc_port``, and
positional binding are out of scope.
"""

import re
from collections import defaultdict

from .model import Graph, Module, Port, Wire

_MODULE_RE = re.compile(r"\bSC_MODULE\s*\(\s*([A-Za-z_]\w*)\s*\)")
_SUBCLASS_RE = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)\s*:\s*public\s+sc_module\b")

# scalar member: kind, optional element type, name, optional {"label"}
_MEMBER_RE = re.compile(
    r"sc_core::(sc_in|sc_out|sc_inout|sc_fifo_in|sc_fifo_out|sc_fifo|sc_signal|sc_clock)"
    r"\s*(?:<([^>]*)>)?\s+([A-Za-z_]\w*)\s*"
    r'(?:\{\s*"([^"]*)"\s*\})?\s*;'
)

# vector member: sc_vector<sc_core::KIND<ELEM>> name{"label", COUNT};
_VECTOR_MEMBER_RE = re.compile(
    r"sc_core::sc_vector\s*<\s*sc_core::(sc_in|sc_out|sc_inout|sc_fifo_in|sc_fifo_out|sc_signal)"
    r"<([^>]*)>\s*>\s+([A-Za-z_]\w*)\s*"
    r'\{\s*"([^"]*)"\s*,\s*([A-Za-z_0-9]+)\s*\}\s*;'
)

# instance.port(member);
_BINDING_RE = re.compile(r"\b([A-Za-z_]\w*)\.(\w+)\s*\(\s*([A-Za-z_]\w*)\s*\)\s*;")

_PROCESS_RE = re.compile(r"\bSC_(METHOD|THREAD|CTHREAD)\s*\(\s*(\w+)\s*\)")
_EVENT_RE = re.compile(r'\brecord\s*\([^,]*,\s*"([a-z_]+)"')

# integer constants used as vector widths, e.g.  inline constexpr unsigned UNIT_COUNT = 2;
_CONST_RE = re.compile(r"\b(?:inline\s+)?(?:constexpr|const)\s+(?:unsigned|int|std::size_t|size_t)\s+(\w+)\s*=\s*(\d+)")

# instance/fifo pointer arrays used in the connect loop:
#   std::array<Compute*, UNIT_COUNT> computeUnits{&m_compute0, &m_compute1};
_ARRAY_RE = re.compile(
    r"\bstd::array\s*<"
)

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


def _balanced_angle(text, open_index):
    """Return index just after the '>' closing the '<' at open_index (handles nesting)."""
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "<":
            depth += 1
        elif text[i] == ">":
            depth -= 1
            if depth == 0:
                return i + 1
    return -1


def _find_arrays(text):
    """Map array name -> element list for ``std::array<T*, N> name{&a, &b};``."""
    arrays = {}
    for match in _ARRAY_RE.finditer(text):
        close = _balanced_angle(text, match.end() - 1)
        if close < 0:
            continue
        tail = re.match(
            r"\s*(\w+)\s*\{\s*((?:&[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?\s*,?\s*)+)\s*\}",
            text[close:],
        )
        if not tail:
            continue
        elements = re.findall(r"&([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?)", tail.group(2))
        if elements and all("." not in e for e in elements):
            arrays[tail.group(1)] = elements
    return arrays


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
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError("unbalanced braces in source")


def _resolve_constants(text):
    return {name: int(value) for name, value in _CONST_RE.findall(text)}


def _expand_vectors_and_loops(text, constants):
    """Return ``text`` with vector members and loop bindings expanded to scalars."""

    def width(token):
        if token.isdigit():
            return int(token)
        return constants.get(token, None)

    # 1) expand sc_vector member declarations into N scalar declarations.
    def expand_vector(match):
        kind, element, name, label, count = match.groups()
        n = width(count)
        if n is None:
            return match.group(0)
        parts = []
        for i in range(n):
            parts.append(f'sc_core::{kind}<{element}> {name}_{i}{{"{label}_{i}"}};')
        return "\n".join(parts)

    text = _VECTOR_MEMBER_RE.sub(expand_vector, text)

    # 2) pointer arrays of instances / fifos for the connect loop.
    arrays = _find_arrays(text)

    n = constants.get("UNIT_COUNT", 2)

    idn = r"([A-Za-z_]\w*)"
    idx = r"\[[A-Za-z_]\w*\]"

    # computeUnits[i]->port(member)
    text = re.sub(
        idn + idx + r"->(\w+)\s*\(\s*" + idn + r"\s*\)\s*;",
        lambda m: _expand_arr_scalar(m, arrays, n),
        text)
    text = re.sub(
        idn + idx + r"->(\w+)\s*\(\s*" + idn + idx + r"\s*\)\s*;",
        lambda m: _expand_arr_indexed(m, arrays, n),
        text)
    text = re.sub(
        idn + idx + r"->(\w+)\s*\(\s*\*" + idn + idx + r"\s*\)\s*;",
        lambda m: _expand_arr_deref(m, arrays, n),
        text)
    # obj.port[i](member)
    text = re.sub(
        idn + r"\.(\w+)" + idx + r"\s*\(\s*" + idn + r"\s*\)\s*;",
        lambda m: _expand_obj_scalar(m, n),
        text)
    text = re.sub(
        idn + r"\.(\w+)" + idx + r"\s*\(\s*" + idn + idx + r"\s*\)\s*;",
        lambda m: _expand_obj_indexed(m, n),
        text)
    text = re.sub(
        idn + r"\.(\w+)" + idx + r"\s*\(\s*\*" + idn + idx + r"\s*\)\s*;",
        lambda m: _expand_obj_deref(m, arrays, n),
        text)

    return text


def _expand_arr_scalar(match, arrays, n):
    array, port, arg = match.group(1), match.group(2), match.group(3)
    if array not in arrays:
        return match.group(0)
    return "\n".join(f"{arrays[array][i]}.{port}({arg});" for i in range(n))


def _expand_arr_indexed(match, arrays, n):
    array, port, base = match.group(1), match.group(2), match.group(3)
    if array not in arrays:
        return match.group(0)
    return "\n".join(f"{arrays[array][i]}.{port}({base}_{i});" for i in range(n))


def _expand_arr_deref(match, arrays, n):
    array, port, farr = match.group(1), match.group(2), match.group(3)
    if array not in arrays or farr not in arrays:
        return match.group(0)
    return "\n".join(f"{arrays[array][i]}.{port}({arrays[farr][i]});" for i in range(n))


def _expand_obj_scalar(match, n):
    obj, port, arg = match.group(1), match.group(2), match.group(3)
    return "\n".join(f"{obj}.{port}_{i}({arg});" for i in range(n))


def _expand_obj_indexed(match, n):
    obj, port, base = match.group(1), match.group(2), match.group(3)
    return "\n".join(f"{obj}.{port}_{i}({base}_{i});" for i in range(n))


def _expand_obj_deref(match, arrays, n):
    obj, port, farr = match.group(1), match.group(2), match.group(3)
    if farr not in arrays:
        return match.group(0)
    return "\n".join(f"{obj}.{port}_{i}({arrays[farr][i]});" for i in range(n))


def _find_blocks(text):
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


def _parse_ports(block):
    ports, processes = [], []
    for match in _MEMBER_RE.finditer(block):
        kind, element, name, label = match.group(1), match.group(2) or "", match.group(3), match.group(4) or ""
        ports.append(Port(name=name, direction=_KIND_DIRECTION.get(kind, "internal"),
                          kind=kind, element=element, label=label))
    for match in _PROCESS_RE.finditer(block):
        processes.append(match.group(2))
    return ports, processes


def _topo_order(instance_names, pairs):
    """Deterministic Kahn topological order over the given (source, target) pairs."""
    from collections import deque

    indegree = {name: 0 for name in instance_names}
    adjacent = {name: [] for name in instance_names}
    for source, target in pairs:
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


def _pretty(channel):
    return channel[2:] if channel.startswith("m_") else channel


def parse(files, top=None, clock_period_ns=1.0):
    """Parse SystemC sources into a :class:`Graph` (see module docstring)."""
    raw = "\n".join(open(path, encoding="utf-8-sig").read() for path in files)
    constants = _resolve_constants(raw)
    text = _expand_vectors_and_loops(_mask_comments(raw), constants)

    blocks = list(_find_blocks(text))
    if not blocks:
        raise ValueError("no SC_MODULE / sc_module module found in input files")

    blocks_by_type = {}
    for name, block in blocks:
        blocks_by_type.setdefault(name, block)
    module_types = set(blocks_by_type)

    ports_by_type = {}
    for name, block in blocks_by_type.items():
        ports, _ = _parse_ports(block)
        ports_by_type[name] = ports

    if not top:
        candidates = []
        for name, block in blocks_by_type.items():
            others = [t for t in module_types
                      if t != name and re.search(r"\b" + t + r"\s+([A-Za-z_]\w*)\s*;", block)]
            if others:
                candidates.append((len(others), name))
        top = max(candidates)[1] if candidates else list(module_types)[-1]

    top_block = blocks_by_type[top]

    clock_match = re.search(r"sc_core::sc_clock\s+([A-Za-z_]\w*)\s*(?:\{\s*\"[^\"]*\"\s*\})?\s*;", top_block)
    clock_name = clock_match.group(1) if clock_match else ""

    channel_re = re.compile(
        r"sc_core::(sc_fifo|sc_signal)<[^>]*>\s+([A-Za-z_]\w*)\s*(?:\{\s*\"[^\"]*\"\s*\})?\s*;"
    )
    fifo_names = set()
    signal_names = set()
    for kind, name in channel_re.findall(top_block):
        (fifo_names if kind == "sc_fifo" else signal_names).add(name)

    instances = {}
    for tpe in sorted(module_types):
        if tpe == top:
            continue
        for match in re.finditer(r"\b" + re.escape(tpe) + r"\s+([A-Za-z_]\w*)\s*;", top_block):
            instances.setdefault(match.group(1), tpe)

    bindings = []
    for match in _BINDING_RE.finditer(text):
        instance_name, port_name, member = match.group(1), match.group(2), match.group(3)
        if instance_name in instances and member.startswith("m_"):
            bindings.append((instance_name, port_name, member))

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

    # group bindings per channel (FIFO or signal), then pair out -> in
    channels = fifo_names | signal_names
    channel_bindings = defaultdict(list)
    for instance_name, port_name, member in bindings:
        if member in channels:
            channel_bindings[member].append((instance_name, port_name))

    edges = defaultdict(list)  # (source, target) -> [(kind, label)]
    for channel, port_bindings in channel_bindings.items():
        sources = [pb for pb in port_bindings if port_dir(pb[0], pb[1]) in ("out", "inout")]
        targets = [pb for pb in port_bindings if port_dir(pb[0], pb[1]) == "in"]
        for src_instance, src_port in sources:
            for dst_instance, dst_port in targets:
                if src_instance != dst_instance:
                    kind = "fifo" if channel in fifo_names else "signal"
                    edges[(src_instance, dst_instance)].append((kind, _pretty(channel)))

    # classify back-pressure edges (ready / base handshakes) and lay out the forward DAG
    back_pairs = set()
    for (source, target), edge_list in edges.items():
        labels = [label for _, label in edge_list]
        if all(kind == "signal" for kind, _ in edge_list) and any(
            "ready" in label.lower() or "base" in label.lower() for label in labels
        ):
            back_pairs.add((source, target))
    order = _topo_order(list(instances), [(s, t) for (s, t) in edges if (s, t) not in back_pairs])

    if clock_name:
        graph.modules.append(Module(id=clock_name, type="Clock"))
    for instance_name in order:
        tpe = instances[instance_name]
        module = Module(id=instance_name, type=tpe)
        for port in ports_by_type.get(tpe, []):
            if port.kind in _PORT_KINDS:
                module.ports.append(port)
        graph.modules.append(module)

    # event name -> module type (longest lowercased-type prefix)
    all_events = sorted(set(_EVENT_RE.findall(raw)))
    events_by_type = {}
    for event in all_events:
        best = None
        for tpe in sorted(module_types - {top}, key=len, reverse=True):
            low = tpe.lower()
            if event.startswith(low) and (best is None or len(low) > len(best[0].lower())):
                best = tpe
        if best:
            events_by_type.setdefault(best, []).append(event)
    for module in graph.modules:
        module.events = events_by_type.get(module.type, [])

    # merge parallel edges into one wire per (source, target)
    for (source, target), edge_list in sorted(edges.items()):
        kinds = {e[0] for e in edge_list}
        kind = "fifo" if "fifo" in kinds else "signal"
        labels = []
        for _, label in edge_list:
            if label not in labels:
                labels.append(label)
        graph.wires.append(Wire(source=source, target=target, kind=kind,
                                source_port="", target_port="",
                                label=" / ".join(labels), back=(source, target) in back_pairs))

    return graph