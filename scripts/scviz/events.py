"""Event CSV loader and interval computation.

The seven core events form the per-task lifecycle and drive the F/T/Q/C/B/O
interval view (same semantics as ``scripts/trace_tui.py``). Stages 2-4 add extra
structural events (``link``, ``compute_unit``, ``reorder_*``, ``window_store``,
``engines_blocked``, ...); those are kept for the per-cycle readout and module
highlighting but do not change the interval rendering.
"""

import csv

EVENT_ORDER = (
    "parser_send", "transform_accept", "transform_emit", "compute_accept",
    "compute_complete", "compute_emit", "output",
)
INTERVAL_SYMBOLS = ("F", "T", "Q", "C", "B", "O")
INTERVAL_LABELS = {
    "F": "input FIFO",
    "T": "Transform",
    "Q": "compute FIFO",
    "C": "Compute",
    "B": "result blocked",
    "O": "output FIFO",
    "*": "output",
}
COLORS = {
    "F": "#22aadd", "T": "#3388ff", "Q": "#e6c300", "C": "#2ca02c",
    "B": "#d62728", "O": "#9467bd", "*": "#eeeeee",
}
EXTRA_COLOR = "#5b6a7d"

EXPECTED_FIELDS = ["id", "event", "cycle", "a", "b", "value", "latency"]


def load_events(path, event_module=None):
    """Return a trace dict from an event CSV.

    ``event_module`` maps event name -> module type name (for graph highlighting).
    Events without a mapping are kept but never highlight a module.
    """
    event_module = event_module or {}
    core = set(EVENT_ORDER)
    tasks = {}
    by_cycle = {}
    all_events = set()
    with open(path, newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != EXPECTED_FIELDS:
            raise ValueError(f"expected header {EXPECTED_FIELDS}, got {reader.fieldnames}")
        for line, row in enumerate(reader, 2):
            name = row["event"]
            values = {key: int(row[key]) for key in ("id", "cycle", "a", "b", "value", "latency")}
            if any(value < 0 for value in values.values()):
                raise ValueError(f"line {line}: negative value")
            cycle = values["cycle"]
            all_events.add(name)
            by_cycle.setdefault(cycle, []).append(name)
            if name in core:
                task = tasks.setdefault(values["id"], {})
                if name in task:
                    raise ValueError(f"line {line}: duplicate event {name!r} for task {values['id']}")
                task[name] = cycle

    end = max((cycle for cycles in by_cycle for cycle in [cycles]), default=0)

    task_rows = []
    for task_id in sorted(tasks):
        task = tasks[task_id]
        intervals = []
        for index, symbol in enumerate(INTERVAL_SYMBOLS):
            if EVENT_ORDER[index] in task:
                start = task[EVENT_ORDER[index]]
                stop = task.get(EVENT_ORDER[index + 1], end + 1)
                intervals.append([start, stop, symbol])
        task_rows.append({"id": task_id, "events": task, "intervals": intervals})

    return {
        "event_order": list(EVENT_ORDER),
        "extra_events": sorted(all_events - core),
        "event_module": event_module,
        "symbols": list(INTERVAL_SYMBOLS),
        "labels": INTERVAL_LABELS,
        "colors": COLORS,
        "extra_color": EXTRA_COLOR,
        "end": end,
        "tasks": task_rows,
        "by_cycle": {cycle: sorted(set(events)) for cycle, events in by_cycle.items()},
    }
