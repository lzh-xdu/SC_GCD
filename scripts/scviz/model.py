"""Data model for the SystemC web viewer."""

from dataclasses import dataclass, field


@dataclass
class Port:
    name: str
    direction: str  # in / out / inout / internal / clock
    kind: str       # sc_in / sc_out / sc_fifo_in / sc_fifo_out / ...
    element: str    # template element type, e.g. bool / RawTask
    label: str = ""  # SystemC object name from {"label"}


@dataclass
class Module:
    id: str          # instance name, e.g. m_parser
    type: str        # module type, e.g. Parser
    ports: list = field(default_factory=list)
    events: list = field(default_factory=list)


@dataclass
class Wire:
    source: str       # node id (module instance or clock name)
    target: str       # node id
    kind: str         # clock / fifo / signal
    source_port: str = ""
    target_port: str = ""
    label: str = ""


@dataclass
class Graph:
    top: str = ""
    clock: str = ""
    clock_period_ns: float = 1.0
    modules: list = field(default_factory=list)
    wires: list = field(default_factory=list)
    files: list = field(default_factory=list)
