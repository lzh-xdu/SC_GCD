"""Dependency-free terminal viewer for the Stage 1 SystemC event CSV."""

import argparse
import bisect
import csv
import os
from pathlib import Path
import shutil
import sys
import time

EVENTS = ("parser_send", "transform_accept", "transform_emit", "compute_accept",
          "compute_complete", "compute_emit", "output")
SYMBOLS = "FTQCBO"
COLORS = {"F": 36, "T": 34, "Q": 33, "C": 32, "B": 31, "O": 35, "*": 97}


class Trace:
    def __init__(self, path):
        self.tasks = {}
        self.events = {}
        self.end = 0
        with open(path, newline="", encoding="utf-8-sig") as stream:
            reader = csv.DictReader(stream)
            if reader.fieldnames != ["id", "event", "cycle", "a", "b", "value", "latency"]:
                raise ValueError("Expected header: id,event,cycle,a,b,value,latency")
            for line, row in enumerate(reader, 2):
                try:
                    name = row["event"]
                    if name not in EVENTS or None in row:
                        raise ValueError("unknown event or extra columns")
                    values = {key: int(row[key]) for key in ("id", "cycle", "a", "b", "value", "latency")}
                    if any(value < 0 for value in values.values()):
                        raise ValueError("negative value")
                    task = self.tasks.setdefault(values["id"], {})
                    index = len(task)
                    if index >= len(EVENTS) or name != EVENTS[index]:
                        raise ValueError("duplicate, missing or out-of-order task event")
                    if index and values["cycle"] < task[EVENTS[index - 1]]["cycle"]:
                        raise ValueError("task cycle moves backwards")
                    task[name] = values
                    self.events.setdefault(values["cycle"], []).append((values["id"], name))
                    self.end = max(self.end, values["cycle"])
                except (ValueError, TypeError, KeyError) as error:
                    raise ValueError(f"CSV line {line}: {error}") from error
        self.ids = sorted(self.tasks)
        self.event_cycles = sorted(self.events)
        self.intervals = {}
        self.starts = {}
        for task_id, task in self.tasks.items():
            intervals = []
            for index, symbol in enumerate(SYMBOLS):
                if EVENTS[index] in task:
                    start = task[EVENTS[index]]["cycle"]
                    stop = task.get(EVENTS[index + 1], {}).get("cycle", self.end + 1)
                    intervals.append((start, stop, symbol))
            self.intervals[task_id] = intervals
            self.starts[task_id] = [part[0] for part in intervals]

    def state(self, task_id, cycle):
        task = self.tasks[task_id]
        if task.get("output", {}).get("cycle") == cycle:
            return "*"
        index = bisect.bisect_right(self.starts[task_id], cycle) - 1
        if index >= 0:
            start, stop, symbol = self.intervals[task_id][index]
            if start <= cycle < stop:
                return symbol
        return "."

    def duration(self, task_id, first, last):
        task = self.tasks[task_id]
        if first not in task or last not in task:
            return None
        return task[last]["cycle"] - task[first]["cycle"]

    def metrics(self):
        complete = [key for key in self.ids if "output" in self.tasks[key]]
        busy = sum(max(0, min(stop, self.end) - start)
                   for parts in self.intervals.values() for start, stop, symbol in parts if symbol == "C")
        wait = sum(max(0, min(stop, self.end) - start)
                   for parts in self.intervals.values() for start, stop, symbol in parts if symbol == "B")
        latencies = [self.duration(key, "parser_send", "output") for key in complete]
        return len(complete), busy, wait, sum(latencies) / len(latencies) if latencies else 0


class View:
    def __init__(self, trace):
        self.trace = trace
        self.cycle = 0
        self.start = 0
        self.scale = 1
        self.selected = 0
        self.playing = False
        self.delay = 0.2

    def render(self, width=120, height=36, color=False):
        trace = self.trace
        columns = max(10, width - 25)
        rows = max(1, height - 23)
        count, busy, wait, average = trace.metrics()
        utilization = busy / trace.end if trace.end else 0
        throughput = count / trace.end if trace.end else 0
        lines = ["SYSTEMC / STAGE 1 TRACE EXPLORER",
                 f"TRACE WINDOW [0,{trace.end}] | tasks {count}/{len(trace.ids)} complete | {'PLAY' if self.playing else 'PAUSE'}",
                 f"Compute {busy} cyc ({utilization:.2%}) | throughput {throughput:.4f} task/cyc | result wait {wait} cyc",
                 f"Mean completed latency {average:.2f} cyc | Host simulation speed: unavailable in event CSV",
                 "F input FIFO | T transform (includes stalls) | Q compute FIFO | C computing | B result blocked | O output FIFO | * output",
                 f"Cycle {self.cycle} (after edge) | view {self.start}..{self.start + (columns - 1) * self.scale} | {self.scale} cyc/column"]
        counts = {symbol: sum(trace.state(key, self.cycle) == symbol for key in trace.ids) for symbol in SYMBOLS}
        lines.append("At cursor: " + "   ".join(f"{name}={counts[symbol]}" for symbol, name in zip(SYMBOLS, ("FIFO P>T", "Transform", "FIFO T>C", "Compute", "Pending", "FIFO C>O"))))
        ruler = [" "] * columns
        for index in range(0, columns, 10):
            label = str(self.start + index * self.scale)
            ruler[index:min(columns, index + len(label))] = label[:columns - index]
        lines.extend([" " * 12 + "".join(ruler), " " * 12 + "".join("^" if self.start + i * self.scale <= self.cycle < self.start + (i + 1) * self.scale else " " for i in range(columns))])
        offset = (self.selected // rows) * rows
        for index in range(offset, min(len(trace.ids), offset + rows)):
            key = trace.ids[index]
            cells = []
            for column in range(columns):
                symbol = trace.state(key, self.start + column * self.scale)
                cells.append(f"\033[{COLORS[symbol]}m{symbol}\033[0m" if color and symbol in COLORS else symbol)
            lines.append(f"{'>' if index == self.selected else ' '} id {key:<6} " + "".join(cells))
        lines.append(f"Tasks {offset + 1 if trace.ids else 0}-{min(len(trace.ids), offset + rows)}/{len(trace.ids)}; zoomed columns sample their LEFT edge (short phases may be hidden).")
        lines.append("-" * min(width - 1, 110))
        if trace.ids:
            key = trace.ids[self.selected]
            task = trace.tasks[key]
            operand = task.get("compute_accept", task.get("transform_emit", {}))
            result = task.get("output", {}).get("value", "pending")
            lines.append(f"SELECTED id={key} | ordered operands=({operand.get('a', '?')}, {operand.get('b', '?')}) | output GCD={result}")
            lines.append("Events: " + " > ".join(f"{name.replace('transform', 'T').replace('compute', 'C').replace('parser', 'P')}@{task[name]['cycle']}" for name in EVENTS if name in task))
            durations = [trace.duration(key, EVENTS[i], EVENTS[i + 1]) for i in range(6)]
            lines.append("Durations: " + " | ".join(f"{symbol}={value if value is not None else '?'}" for symbol, value in zip(SYMBOLS, durations)))
            extra = max(0, durations[1] - 2) if durations[1] is not None else "?"
            lines.append(f"Transform above 2-cycle baseline: {extra} cyc | end-to-end: {trace.duration(key, 'parser_send', 'output')} cyc")
        else:
            lines.append("Empty trace: no events; simulation end and EOF overhead cannot be inferred.")
        events = trace.events.get(self.cycle, [])
        lines.append("Cursor events: " + (", ".join(f"#{key} {name}" for key, name in events) or "none"))
        lines.extend(["[a/d] cycle -/+  [n/p] next/prev event  [w/s] select task  [j] jump to task start",
                      "[h/l] pan  [+/-] zoom in/out  [f] fit  [0] reset  [Space] play/pause  [q] quit",
                      "Intervals use [start,end); zero-cycle phases remain visible in selected event details.",
                      "Metrics use last EVENT as denominator, not verified run end. No FIFO capacity or host timing in this file."])
        # Clip plain text first; colored timeline cells have a known visible width.
        return "\n".join(line if "\033[" in line else line[:max(1, width - 1)] for line in lines)

    def handle(self, key, width=120):
        if not key:
            return True
        columns = max(10, width - 25)
        if key == "q":
            return False
        if key == " ":
            self.playing = not self.playing
        elif key in ("a", "d"):
            self.cycle += 1 if key == "d" else -1
        elif key in ("n", "p"):
            cycles = self.trace.event_cycles
            index = bisect.bisect_right(cycles, self.cycle) if key == "n" else bisect.bisect_left(cycles, self.cycle) - 1
            if 0 <= index < len(cycles):
                self.cycle = cycles[index]
        elif key in ("w", "s"):
            self.selected = min(max(0, self.selected + (1 if key == "s" else -1)), max(0, len(self.trace.ids) - 1))
        elif key == "j" and self.trace.ids:
            self.cycle = self.trace.tasks[self.trace.ids[self.selected]]["parser_send"]["cycle"]
        elif key in ("+", "=", "-"):
            self.scale = max(1, self.scale // 2) if key != "-" else min(max(1, self.trace.end), self.scale * 2)
            self.start = self.cycle
        elif key == "f":
            self.start = 0
            self.scale = max(1, (self.trace.end + columns) // columns)
        elif key in ("h", "l"):
            self.start = max(0, min(self.trace.end, self.start + (1 if key == "l" else -1) * max(1, columns // 2) * self.scale))
            return True
        elif key == "0":
            self.cycle = self.start = 0
            self.scale = 1
        self.cycle = min(self.trace.end, max(0, self.cycle))
        if key not in ("f", "w", "s") and not self.start <= self.cycle < self.start + columns * self.scale:
            self.start = self.cycle
        return True


def interactive(view):
    previous = None
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes
        import msvcrt
        kernel = ctypes.windll.kernel32
        kernel.GetStdHandle.argtypes = [wintypes.DWORD]
        kernel.GetStdHandle.restype = wintypes.HANDLE
        kernel.GetConsoleMode.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel.SetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        handle = kernel.GetStdHandle(-11)
        mode = ctypes.c_ulong()
        if not kernel.GetConsoleMode(handle, ctypes.byref(mode)) or not kernel.SetConsoleMode(handle, mode.value | 4):
            raise ValueError("Terminal does not support ANSI; use Windows Terminal or --snapshot")
        previous = mode.value

        def read_key():
            if not msvcrt.kbhit():
                return ""
            key = msvcrt.getwch()
            if key in ("\x00", "\xe0"):
                msvcrt.getwch()
                return ""
            return key
    else:
        import select
        import termios
        import tty
        previous = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())

        def read_key():
            return sys.stdin.read(1) if select.select([sys.stdin], [], [], 0)[0] else ""
    try:
        sys.stdout.write("\033[?1049h\033[?25l")
        last_frame = ""
        next_tick = time.monotonic()
        while True:
            size = shutil.get_terminal_size((120, 36))
            key = read_key()
            if key == "\x03" or not view.handle(key, size.columns):
                break
            if view.playing and time.monotonic() >= next_tick:
                view.handle("d", size.columns)
                next_tick = time.monotonic() + view.delay
                if view.cycle == view.trace.end:
                    view.playing = False
            frame = ("Please resize terminal to at least 80 columns x 24 rows. [q] quit"
                     if size.columns < 80 or size.lines < 24 else view.render(size.columns, size.lines, True))
            if frame != last_frame:
                sys.stdout.write("\033[H" + frame.replace("\n", "\033[K\n") + "\033[K\033[J")
                sys.stdout.flush()
                last_frame = frame
            time.sleep(0.03)
    finally:
        sys.stdout.write("\033[0m\033[?25h\033[?1049l")
        sys.stdout.flush()
        if os.name == "nt":
            kernel.SetConsoleMode(handle, previous)
        else:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, previous)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="?", type=Path, default=Path(__file__).resolve().parents[1] / "build/debug/stage1-events.csv")
    parser.add_argument("--snapshot", action="store_true", help="print a plain-text frame without interactive terminal")
    parser.add_argument("--cycle", type=int, default=0)
    parser.add_argument("--width", type=int, default=140, help="snapshot columns")
    args = parser.parse_args()
    try:
        view = View(Trace(args.csv))
        view.cycle = max(0, min(view.trace.end, args.cycle))
        if args.snapshot:
            print(view.render(max(100, args.width), 40))
        elif not sys.stdin.isatty() or not sys.stdout.isatty():
            parser.error("Interactive mode requires a terminal. Run in PowerShell, or add --snapshot.")
        else:
            interactive(view)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Trace error: {error}\n")
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
