"""Event CSV loader and interval computation.

Reuses the semantics of ``scripts/trace_tui.py`` so the web animation matches the
existing terminal viewer: the seven stage events form six half-open intervals
``[start, stop)``, coloured F / T / Q / C / B / O, plus an ``*`` marker for the
final ``output`` event.
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

EXPECTED_FIELDS = ["id", "event", "cycle", "a", "b", "value", "latency"]


def load_events(path, event_module=None):
    """Return a trace dict from an event CSV.

    ``event_module`` maps each event name to a module type name (for graph
    highlighting); events with no mapping are kept but never highlight a module.
    """
    event_module = event_module or {}
    tasks = {}
    with open(path, newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != EXPECTED_FIELDS:
            raise ValueError(f"expected header {EXPECTED_FIELDS}, got {reader.fieldnames}")
        for line, row in enumerate(reader, 2):
            name = row["event"]
            if name not in EVENT_ORDER:
                raise ValueError(f"line {line}: unknown event {name!r}")
            values = {key: int(row[key]) for key in ("id", "cycle", "a", "b", "value", "latency")}
            if any(value < 0 for value in values.values()):
                raise ValueError(f"line {line}: negative value")
            task = tasks.setdefault(values["id"], {})
            if name in task:
                raise ValueError(f"line {line}: duplicate event {name!r} for task {values['id']}")
            task[name] = values["cycle"]

    end = max((cycle for task in tasks.values() for cycle in task.values()), default=0)

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
        "event_module": event_module,
        "symbols": list(INTERVAL_SYMBOLS),
        "labels": INTERVAL_LABELS,
        "colors": COLORS,
        "end": end,
        "tasks": task_rows,
    }
