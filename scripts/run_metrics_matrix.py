"""Run the stage 1-4 metrics matrix defined in docs/metrics-presentation.md.

Collects raw per-run statistics (runs.csv), per-resource occupancy
(resources.csv), host measurements (profiling_samples.csv), offline blocked
segment analysis, backpressure/ordering timelines and the summary tables and
SVG plots used by docs/metrics-results.md.

Host measurements use Linux process scope (wall clock around the process,
CPU time and peak RSS from os.wait4). The entry-scope model_ms numbers in
docs/model-comparison.md come from the Windows profiling wrappers and remain
the primary baseline; this script re-uses the same scenario names and seeds.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import platform
import random
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from metrics_svg import grouped_bar_chart, line_chart

MAX_CYCLES = 1000000000
LONG_PAIR = (1836311903, 1134903170)


def scenarios():
    rng = random.Random(20260910)
    mixed = [(rng.randint(-2**31, 2**31 - 1), rng.randint(-2**31, 2**31 - 1)) for _ in range(10000)]
    return {
        "empty": dict(tasks=[], period=1, note="空输入边界"),
        "single": dict(tasks=[(48, 18)], period=1, note="单任务边界"),
        "zero_delay": dict(tasks=[(7, 0)] * 30000, period=1, note="零计算延迟(b=0)"),
        "short": dict(tasks=[(48, 18)] * 10000, period=1, note="连续短任务(L=6)"),
        "long": dict(tasks=[LONG_PAIR] * 10000, period=1, note="连续长任务(斐波那契对)"),
        "mixed": dict(tasks=mixed, period=1, note="固定种子混合负载"),
        "skew": dict(tasks=([LONG_PAIR] + [(7, 0)] * 20) * 500, period=1, note="首项长后续短"),
        "slow_sink": dict(tasks=[(48, 18)] * 2000, period=1000, note="慢输出(节拍1000)"),
        "sparse_wait": dict(tasks=[(48, 18)], period=2000000, note="稀疏长等待"),
        "slow_sink_small": dict(tasks=[(48, 18)] * 24, period=8, note="背压时间线小样本"),
        "skew_small": dict(tasks=([LONG_PAIR] + [(7, 0)] * 20) * 2, period=1, note="保序时间线小样本"),
        "dense_small": dict(tasks=[(7, 0)] * 2000, period=1, note="Trace开销短任务"),
        "mixed_small": dict(tasks=mixed[:1000], period=1, note="Trace开销混合任务"),
    }


A1_CASES = ["empty", "single", "zero_delay", "short", "long", "mixed"]
A2_CASES = A1_CASES + ["slow_sink"]
A3_CASES = ["empty", "single", "zero_delay", "short", "long", "mixed", "skew"]
A4_EQUIV_CASES = A3_CASES + ["slow_sink", "sparse_wait"]
A4_EFFICIENCY_CASES = ["zero_delay", "long", "mixed", "slow_sink", "skew", "sparse_wait"]
A4_TRACE_CASES = ["dense_small", "mixed_small"]

MODEL_EXTRA = {
    "stage1": (),
    "stage2": (),
    "stage3": ("result_depth",),
    "stage3_window": ("result_depth", "window", "seed"),
    "stage4": ("result_depth", "window", "seed"),
}


class Cfg:
    def __init__(self, period=1, depth=2, result_depth=2, window=8, seed=7, trace=False):
        self.period, self.depth, self.result_depth = period, depth, result_depth
        self.window, self.seed, self.trace = window, seed, trace
        self.max_cycles = MAX_CYCLES


def digest(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def oracle(tasks):
    return [math.gcd(abs(a), abs(b)) for a, b in tasks]


class Runner:
    def __init__(self, build: Path, work: Path):
        self.build, self.work = build, work
        self.runs = []
        self.resources = []

    def tag(self, model, case, cfg):
        parts = [model, case, f"D{cfg.depth}", f"P{cfg.period}"]
        if "result_depth" in MODEL_EXTRA[model]:
            parts.append(f"R{cfg.result_depth}")
        if "window" in MODEL_EXTRA[model]:
            parts += [f"W{cfg.window}", f"S{cfg.seed}"]
        parts.append("trace" if cfg.trace else "notrace")
        return "-".join(parts)

    def command(self, model, cfg, paths):
        args = [str(self.build / f"{model}_gcd"), str(paths["input"]), str(paths["output"]),
                str(paths["stats"]), str(cfg.depth),
                str(paths["events"]) if cfg.trace else "-", str(cfg.period), str(cfg.max_cycles)]
        for key in MODEL_EXTRA[model]:
            args.append(str(getattr(cfg, key)))
        return args

    def run(self, model, case, cfg, tasks, assignment):
        folder = self.work / self.tag(model, case, cfg)
        folder.mkdir(parents=True, exist_ok=True)
        paths = {name: folder / f"{name}.txt" if name == "output" else folder / f"{name}.csv"
                 for name in ("input", "output", "stats", "events")}
        paths["input"] = folder / "input.txt"
        paths["input"].write_text("".join(f"{a} {b}\n" for a, b in tasks), encoding="utf-8")
        for key in ("output", "stats", "events"):
            paths[key].unlink(missing_ok=True)
        started = time.perf_counter()
        result = subprocess.run(self.command(model, cfg, paths), capture_output=True, text=True, timeout=300)
        wall_ms = (time.perf_counter() - started) * 1000
        (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(f"{self.tag(model, case, cfg)} failed: {result.stderr[-2000:]}")
        expected = oracle(tasks)
        actual = [int(line) for line in paths["output"].read_text().splitlines()]
        if actual != expected:
            raise AssertionError(f"oracle mismatch: {model}/{case}")
        metrics = parse_stats(paths["stats"])
        run_id = len(self.runs) + 1
        reserved = {"run_id", "assignment", "model", "case", "tasks", "depth", "result_depth", "window",
                    "seed", "period", "trace", "wall_ms", "input_sha256", "output_sha256", "stats_sha256"}
        record = dict(run_id=run_id, assignment=assignment, model=model, case=case, tasks=len(tasks),
                      depth=cfg.depth, result_depth=cfg.result_depth if "result_depth" in MODEL_EXTRA[model] else "",
                      window=cfg.window if "window" in MODEL_EXTRA[model] else "",
                      seed=cfg.seed if "window" in MODEL_EXTRA[model] else "",
                      period=cfg.period, trace=int(cfg.trace), wall_ms=round(wall_ms, 3),
                      input_sha256=digest(paths["input"]), output_sha256=digest(paths["output"]),
                      stats_sha256=digest(paths["stats"]))
        record.update({key: value for key, value in metrics.items() if key not in reserved})
        self.runs.append(record)
        self.collect_resources(record)
        return record, paths

    def collect_resources(self, record):
        model = record["model"]
        fifos = {"stage1": ["parser_to_transform", "transform_to_compute", "compute_to_output"],
                 "stage2": ["parser_to_transform", "compute_to_output"]}.get(
                     model, ["parser_to_transform", "compute0_results", "compute1_results", "collector_to_output"])
        capacities = {name: (record["result_depth"] if name.endswith("_results") else record["depth"])
                      for name in fifos}
        for name, capacity in capacities.items():
            self.resources.append(self.resource_row(record, name, capacity, "fifo"))
        self.resources.append(self.resource_row(record, "transform_registers", 2, "registers"))
        if model in ("stage3_window", "stage4"):
            self.resources.append(self.resource_row(record, "window_results", record["window"], "window_slots"))
            self.resources.append(self.resource_row(record, "window_reserved", record["window"], "window_credits"))

    def resource_row(self, record, name, capacity, kind):
        average, peak = record.get(f"{name}_average"), record.get(f"{name}_peak")
        row = dict(run_id=record["run_id"], model=record["model"], case=record["case"],
                   assignment=record["assignment"], resource=name, kind=kind, capacity=capacity,
                   average=average, peak=peak)
        row["average_rate"] = average / capacity if capacity and average is not None else ""
        row["peak_rate"] = peak / capacity if capacity and peak is not None else ""
        return row


def parse_stats(path: Path):
    metrics = {}
    with path.open() as stream:
        for row in csv.reader(stream):
            if len(row) == 2 and row[0] != "metric":
                metrics[row[0]] = float(row[1])
    return metrics


def blocked_segments(events_path: Path):
    """Offline consecutive-block analysis from per-cycle link events (value=ready)."""
    blocked_cycles = set()
    with events_path.open() as stream:
        for row in csv.DictReader(stream):
            if row["event"] == "link" and int(row["value"]) == 0:
                blocked_cycles.add(int(row["cycle"]))
    if not blocked_cycles:
        return dict(segments=0, mean_cycles=0.0, max_cycles=0, total_cycles=0)
    ordered = sorted(blocked_cycles)
    segments, start = [], ordered[0]
    for previous, current in zip(ordered, ordered[1:]):
        if current != previous + 1:
            segments.append(previous - start + 1)
            start = current
    segments.append(ordered[-1] - start + 1)
    return dict(segments=len(segments), mean_cycles=statistics.mean(segments),
                max_cycles=max(segments), total_cycles=len(ordered))


def timeline_rows(events_path: Path, columns):
    by_id = {}
    with events_path.open() as stream:
        for row in csv.DictReader(stream):
            by_id.setdefault(int(row["id"]), {})[row["event"]] = int(row["cycle"])
    return [dict(id=task_id, **{name: events.get(name, "") for name in columns})
            for task_id, events in sorted(by_id.items())]


def skew_timeline(events_path: Path):
    unit_of, rows = {}, {}
    with events_path.open() as stream:
        for row in csv.DictReader(stream):
            task_id = int(row["id"])
            entry = rows.setdefault(task_id, {})
            if row["event"] == "compute_unit":
                unit_of[task_id] = int(row["a"])
            elif row["event"] in ("compute_accept", "compute_complete", "window_store", "reorder_emit"):
                entry[row["event"]] = int(row["cycle"])
    result = []
    for task_id, entry in sorted(rows.items()):
        row = dict(id=task_id, unit=unit_of.get(task_id, ""), **entry)
        if "window_store" in entry and "reorder_emit" in entry:
            row["window_residence"] = entry["reorder_emit"] - entry["window_store"]
        result.append(row)
    return result


def measure_process(command, log_path: Path):
    """Run one process; return wall ms, CPU ms and peak RSS bytes (Linux wait4)."""
    with log_path.open("wb") as log:
        started = time.perf_counter()
        process = subprocess.Popen(command, stdout=log, stderr=log)
        _, status, usage = os.wait4(process.pid, 0)
        wall_ms = (time.perf_counter() - started) * 1000
    process.returncode = os.WEXITSTATUS(status)
    if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
        raise RuntimeError(f"profiling run failed: exit={status}, log={log_path}")
    return wall_ms, (usage.ru_utime + usage.ru_stime) * 1000, usage.ru_maxrss * 1024


def parse_scheduler_fields(log_path: Path):
    text = log_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"EVENT_SCHEDULER activations=(\d+) max_jump_cycles=(\d+)", text)
    return (int(match.group(1)), int(match.group(2))) if match else ("", "")


class Profiler:
    def __init__(self, runner: Runner):
        self.runner = runner
        self.samples = []

    def profile(self, model, case, cfg, tasks, repetitions, phase_note):
        folder = self.runner.work / ("profile-" + self.runner.tag(model, case, cfg))
        folder.mkdir(parents=True, exist_ok=True)
        paths = {"input": folder / "input.txt", "output": folder / "output.txt",
                 "stats": folder / "stats.csv", "events": folder / "events.csv"}
        paths["input"].write_text("".join(f"{a} {b}\n" for a, b in tasks), encoding="utf-8")
        command = self.runner.command(model, cfg, paths)
        expected = oracle(tasks)
        reference = None
        for phase in ["warmup"] + [f"rep{index}" for index in range(repetitions)]:
            paths["output"].unlink(missing_ok=True)
            paths["stats"].unlink(missing_ok=True)
            paths["events"].unlink(missing_ok=True)
            log_path = folder / f"{phase}.log"
            wall_ms, cpu_ms, peak_rss = measure_process(command, log_path)
            actual = [int(line) for line in paths["output"].read_text().splitlines()]
            if actual != expected:
                raise AssertionError(f"oracle mismatch while profiling {model}/{case}/{phase}")
            hashes = tuple(digest(paths[key]) for key in ("output", "stats", "events") if cfg.trace or key != "events")
            if reference is not None and hashes != reference:
                raise AssertionError(f"nondeterministic output while profiling {model}/{case}/{phase}")
            reference = hashes
            activations, max_jump = parse_scheduler_fields(log_path)
            self.samples.append(dict(case=case, model=model, trace=int(cfg.trace), phase=phase,
                                     wall_ms=round(wall_ms, 3), cpu_ms=round(cpu_ms, 3),
                                     peak_rss_bytes=peak_rss, trace_bytes=paths["events"].stat().st_size if cfg.trace else 0,
                                     activations=activations, max_jump_cycles=max_jump))
        return paths


def write_csv(path: Path, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fieldnames = []
    for row in rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def get(run, key):
    value = run.get(key, "")
    return value if value != "" else 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build-linux"))
    parser.add_argument("--work", type=Path, default=Path("tmp/metrics-matrix"))
    parser.add_argument("--report", type=Path, default=Path("docs/evidence/metrics-matrix"))
    parser.add_argument("--repeat", type=int, default=7)
    options = parser.parse_args()
    build, work, report = (path.resolve() for path in (options.build, options.work, options.report))
    work.mkdir(parents=True, exist_ok=True)
    report.mkdir(parents=True, exist_ok=True)
    all_scenarios = scenarios()
    runner = Runner(build, work)
    by_key = {}

    def record_run(model, case, cfg, assignment):
        record, paths = runner.run(model, case, cfg, all_scenarios[case]["tasks"], assignment)
        by_key[(model, case, cfg.depth, cfg.result_depth, cfg.window, cfg.period)] = record
        print(f"run {record['run_id']:3d} {model:13s} {case:15s} T={int(get(record, 'cycles'))}", flush=True)
        return record, paths

    # ---- Assignment 1: stage 1 baseline -----------------------------------
    for case in A1_CASES:
        record_run("stage1", case, Cfg(period=all_scenarios[case]["period"]), 1)

    # ---- Assignment 2: stage 2 base + depth/period sweeps -----------------
    for case in A2_CASES:
        record_run("stage2", case, Cfg(period=all_scenarios[case]["period"]), 2)
    depth_runs = []
    for depth in (1, 2, 4, 8, 16, 32):
        depth_runs.append(record_run("stage2", "mixed", Cfg(depth=depth, trace=True), 2)[0])
    period_runs = []
    for period in (1, 2, 4, 8, 16):
        period_runs.append(record_run("stage2", "short", Cfg(period=period, trace=True), 2)[0])
    timeline_sink, sink_paths = record_run("stage2", "slow_sink_small", Cfg(period=8, trace=True), 2)

    # ---- Assignment 3: dual instance and ordering -------------------------
    for case in A3_CASES:
        record_run("stage2", case, Cfg(period=all_scenarios[case]["period"]), 3)
    for case in A3_CASES:
        record_run("stage3", case, Cfg(period=all_scenarios[case]["period"]), 3)
        record_run("stage3_window", case, Cfg(period=all_scenarios[case]["period"]), 3)
    window_runs = []
    for window in (1, 2, 4, 8, 16, 32):
        window_runs.append(record_run("stage3_window", "skew", Cfg(window=window), 3)[0])
    result_depth_runs = []
    for result_depth in (1, 2, 4, 8):
        result_depth_runs.append(
            record_run("stage3_window", "mixed", Cfg(result_depth=result_depth), 3)[0])
    timeline_skew, skew_paths = record_run("stage3_window", "skew_small", Cfg(trace=True), 3)

    # ---- Assignment 4: equivalence ----------------------------------------
    equivalence = []
    for case in A4_EQUIV_CASES:
        cfg = Cfg(period=all_scenarios[case]["period"])
        clock, _ = record_run("stage3_window", case, cfg, 4)
        event, _ = record_run("stage4", case, cfg, 4)
        equivalence.append(dict(case=case, tasks=int(get(clock, "tasks")),
                                cycles_clock=int(get(clock, "cycles")), cycles_event=int(get(event, "cycles")),
                                output_identical=clock["output_sha256"] == event["output_sha256"],
                                stats_identical=clock["stats_sha256"] == event["stats_sha256"]))

    # ---- Assignment 4: host efficiency and trace overhead ------------------
    profiler = Profiler(runner)
    efficiency = {}
    for case in A4_EFFICIENCY_CASES:
        cfg = Cfg(period=all_scenarios[case]["period"])
        for model in ("stage3_window", "stage4"):
            profiler.profile(model, case, cfg, all_scenarios[case]["tasks"], options.repeat, "efficiency")
    for case in A4_TRACE_CASES:
        for trace in (False, True):
            cfg = Cfg(period=all_scenarios[case]["period"], trace=trace)
            for model in ("stage3_window", "stage4"):
                profiler.profile(model, case, cfg, all_scenarios[case]["tasks"], options.repeat, "trace")
    # Byte-identical trace comparison between the two models.
    for case in A4_TRACE_CASES:
        clock_trace = work / ("profile-" + runner.tag("stage3_window", case, Cfg(trace=True))) / "events.csv"
        event_trace = work / ("profile-" + runner.tag("stage4", case, Cfg(trace=True))) / "events.csv"
        equivalence.append(dict(case=case, tasks=len(all_scenarios[case]["tasks"]), cycles_clock="",
                                cycles_event="", output_identical=True, stats_identical=True,
                                trace_identical=clock_trace.read_bytes() == event_trace.read_bytes()))

    # ---- Offline analysis: blocked segments and timelines ------------------
    segments = []
    for record in depth_runs + period_runs + [timeline_sink]:
        folder = work / runner.tag(record["model"], record["case"],
                                   Cfg(depth=record["depth"], period=record["period"], trace=True))
        analysis = blocked_segments(folder / "events.csv")
        if analysis["total_cycles"] != int(get(record, "handshake_blocked_cycles")):
            raise AssertionError(f"segment total mismatch for run {record['run_id']}")
        segments.append(dict(run_id=record["run_id"], model=record["model"], case=record["case"],
                             depth=record["depth"], period=record["period"], **analysis))
    timeline_columns = ["parser_send", "transform_accept", "transform_emit", "compute_accept",
                        "compute_complete", "compute_emit", "output"]
    sink_timeline = timeline_rows(sink_paths["events"], timeline_columns)
    for row in sink_timeline:
        if row["compute_emit"] != "" and row["compute_complete"] != "":
            row["result_wait"] = row["compute_emit"] - row["compute_complete"]
    skew_table = skew_timeline(skew_paths["events"])

    # ---- Evidence files ----------------------------------------------------
    write_csv(report / "runs.csv", runner.runs)
    write_csv(report / "resources.csv", runner.resources)
    write_csv(report / "profiling_samples.csv", profiler.samples)
    write_csv(report / "blocked_segments.csv", segments)
    write_csv(report / "a4_equivalence.csv", equivalence)
    write_csv(report / "timeline_slow_sink.csv", sink_timeline)
    write_csv(report / "timeline_skew.csv", skew_table)
    environment = {
        "git_head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        "kernel": platform.release(), "wsl": "microsoft" in Path("/proc/version").read_text().lower(),
        "cpu": next(line.split(":", 1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                    if line.startswith("model name")),
        "logical_processors": os.cpu_count(), "python": platform.python_version(),
        "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
        "build_type": "Release", "repeat": options.repeat, "warmups": 1,
        "host_measurement_scope": "process: wall clock incl. startup; CPU and peak RSS from wait4",
        "binaries": {model: digest(build / f"{model}_gcd") for model in MODEL_EXTRA},
    }
    (report / "environment.json").write_text(json.dumps(environment, indent=2) + "\n", encoding="utf-8")

    # ---- Summary tables ------------------------------------------------------
    summarize(runner, profiler, segments, report, by_key, depth_runs, period_runs, window_runs,
              result_depth_runs)
    print(f"\nDONE: {len(runner.runs)} runs, {len(profiler.samples)} profile samples -> {report}")


def summarize(runner, profiler, segments, report, by_key, depth_runs, period_runs, window_runs,
              result_depth_runs):
    run_of = lambda model, case, **kw: by_key[(model, case, kw.get("depth", 2), kw.get("result_depth", 2),
                                               kw.get("window", 8), kw.get("period", 1))]

    # Assignment 1 overview.
    a1 = []
    for case in A1_CASES:
        run = run_of("stage1", case, period=scenarios()[case]["period"])
        a1.append(dict(case=case, tasks=int(get(run, "tasks")), cycles=int(get(run, "cycles")),
                       throughput=get(run, "throughput_tasks_per_cycle"),
                       busy_cycles=int(get(run, "compute_busy_cycles")), utilization=get(run, "compute_utilization"),
                       idle_no_input_cycles=int(get(run, "compute0_idle_no_input_cycles")),
                       result_wait_cycles=int(get(run, "compute_result_wait_cycles")),
                       e2e_mean=get(run, "task_latency_end_to_end_mean_cycles"),
                       e2e_p95=get(run, "task_latency_end_to_end_p95_cycles"),
                       e2e_max=get(run, "task_latency_end_to_end_max_cycles")))
    write_csv(report / "a1_overview.csv", a1)

    # Assignment 2 sweep tables.
    segment_of = {(row["model"], row["case"], row["depth"], row["period"]): row for row in segments}
    a2_depth = []
    for run in depth_runs:
        seg = segment_of[("stage2", "mixed", run["depth"], run["period"])]
        a2_depth.append(dict(depth=run["depth"], cycles=int(get(run, "cycles")), tasks=int(get(run, "tasks")),
                             throughput=get(run, "throughput_tasks_per_cycle"),
                             blocked_cycles=int(get(run, "handshake_blocked_cycles")),
                             blocked_over_valid=get(run, "handshake_blocked_fraction"),
                             blocked_over_total=get(run, "handshake_blocked_cycles") / get(run, "cycles"),
                             result_wait_cycles=int(get(run, "compute_result_wait_cycles")),
                             e2e_p95=get(run, "task_latency_end_to_end_p95_cycles"),
                             blocked_segments=seg["segments"], blocked_segment_max=seg["max_cycles"]))
    write_csv(report / "a2_depth.csv", a2_depth)
    a2_period = []
    for run in period_runs:
        seg = segment_of[("stage2", "short", run["depth"], run["period"])]
        a2_period.append(dict(period=run["period"], cycles=int(get(run, "cycles")), tasks=int(get(run, "tasks")),
                              throughput=get(run, "throughput_tasks_per_cycle"),
                              blocked_cycles=int(get(run, "handshake_blocked_cycles")),
                              blocked_over_valid=get(run, "handshake_blocked_fraction"),
                              blocked_over_total=get(run, "handshake_blocked_cycles") / get(run, "cycles"),
                              result_wait_cycles=int(get(run, "compute_result_wait_cycles")),
                              e2e_p95=get(run, "task_latency_end_to_end_p95_cycles"),
                              blocked_segments=seg["segments"], blocked_segment_max=seg["max_cycles"]))
    write_csv(report / "a2_period.csv", a2_period)

    # Assignment 3 single/dual comparison.
    a3 = []
    for case in A3_CASES:
        period = scenarios()[case]["period"]
        single = run_of("stage2", case, period=period)
        dual = run_of("stage3", case, period=period)
        window_model = run_of("stage3_window", case, period=period)
        t_single, t_dual, t_window = get(single, "cycles"), get(dual, "cycles"), get(window_model, "cycles")
        busy0, busy1 = get(window_model, "compute0_busy_cycles"), get(window_model, "compute1_busy_cycles")
        a3.append(dict(case=case, tasks=int(get(single, "tasks")), t_single=int(t_single), t_dual=int(t_dual),
                       t_window=int(t_window), speedup_dual=t_single / t_dual if t_dual else "",
                       efficiency_dual=t_single / t_dual / 2 if t_dual else "",
                       speedup_window=t_single / t_window if t_window else "",
                       efficiency_window=t_single / t_window / 2 if t_window else "",
                       tasks0=int(get(window_model, "compute0_tasks")), tasks1=int(get(window_model, "compute1_tasks")),
                       busy0=int(busy0), busy1=int(busy1), utilization0=get(window_model, "compute0_utilization"),
                       utilization1=get(window_model, "compute1_utilization"),
                       imbalance=abs(busy0 - busy1) / (busy0 + busy1) if busy0 + busy1 else ""))
    write_csv(report / "a3_compare.csv", a3)

    a3_window = []
    for run in window_runs:
        window = int(run["window"])
        a3_window.append(dict(window=window, cycles=int(get(run, "cycles")), throughput=get(run, "throughput_tasks_per_cycle"),
                              reorder_wait_cycles=int(get(run, "reorder_wait_cycles")),
                              reorder_wait_over_total=get(run, "reorder_wait_cycles") / get(run, "cycles"),
                              window_blocked_cycles=int(get(run, "window_blocked_cycles")),
                              window_blocked_with_ready=int(get(run, "window_blocked_with_ready_cycles")),
                              reserved_peak=int(get(run, "window_reserved_peak")),
                              reserved_average=get(run, "window_reserved_average"),
                              results_peak=int(get(run, "window_results_peak")),
                              results_average=get(run, "window_results_average"),
                              window_bits=96 * window + window,
                              buffer_payload_bits=int(get(run, "buffer_payload_bits"))))
    write_csv(report / "a3_window.csv", a3_window)

    a3_result_depth = []
    for run in result_depth_runs:
        a3_result_depth.append(dict(result_depth=int(run["result_depth"]), cycles=int(get(run, "cycles")),
                                    throughput=get(run, "throughput_tasks_per_cycle"),
                                    result_wait_cycles=int(get(run, "compute_result_wait_cycles")),
                                    busy0=int(get(run, "compute0_busy_cycles")), busy1=int(get(run, "compute1_busy_cycles")),
                                    utilization0=get(run, "compute0_utilization"),
                                    utilization1=get(run, "compute1_utilization")))
    write_csv(report / "a3_result_depth.csv", a3_result_depth)

    # Assignment 4 efficiency and trace overhead.
    formal = [row for row in profiler.samples if row["phase"] != "warmup"]

    def quartiles(case, model, trace, key):
        values = [row[key] for row in formal if row["case"] == case and row["model"] == model
                  and row["trace"] == trace]
        middle = statistics.median(values)
        first, _, third = statistics.quantiles(values, n=4, method="inclusive")
        return middle, first, third, values

    a4 = []
    for case in A4_EFFICIENCY_CASES:
        tasks = len(scenarios()[case]["tasks"])
        clock_ms, clock_q1, clock_q3, _ = quartiles(case, "stage3_window", 0, "wall_ms")
        event_ms, event_q1, event_q3, _ = quartiles(case, "stage4", 0, "wall_ms")
        clock_cpu, _, _, _ = quartiles(case, "stage3_window", 0, "cpu_ms")
        event_cpu, _, _, _ = quartiles(case, "stage4", 0, "cpu_ms")
        clock_rss, _, _, _ = quartiles(case, "stage3_window", 0, "peak_rss_bytes")
        event_rss, _, _, _ = quartiles(case, "stage4", 0, "peak_rss_bytes")
        event_activations = [row["activations"] for row in formal if row["case"] == case
                             and row["model"] == "stage4"][0]
        a4.append(dict(case=case, tasks=tasks, clock_wall_ms=round(clock_ms, 2), clock_q1=round(clock_q1, 2),
                       clock_q3=round(clock_q3, 2), event_wall_ms=round(event_ms, 2), event_q1=round(event_q1, 2),
                       event_q3=round(event_q3, 2), speedup=round(clock_ms / event_ms, 2),
                       clock_cpu_ms=round(clock_cpu, 2), event_cpu_ms=round(event_cpu, 2),
                       clock_peak_rss_mib=round(clock_rss / 2**20, 2), event_peak_rss_mib=round(event_rss / 2**20, 2),
                       tasks_per_host_second_clock=round(tasks / (clock_ms / 1000)),
                       tasks_per_host_second_event=round(tasks / (event_ms / 1000)),
                       event_activations=event_activations))
    write_csv(report / "a4_efficiency.csv", a4)

    a4_trace = []
    for case in A4_TRACE_CASES:
        tasks = len(scenarios()[case]["tasks"])
        for model in ("stage3_window", "stage4"):
            off, _, _, _ = quartiles(case, model, 0, "wall_ms")
            on, _, _, _ = quartiles(case, model, 1, "wall_ms")
            off_rss, _, _, _ = quartiles(case, model, 0, "peak_rss_bytes")
            on_rss, _, _, _ = quartiles(case, model, 1, "peak_rss_bytes")
            trace_bytes = [row["trace_bytes"] for row in formal if row["case"] == case
                           and row["model"] == model and row["trace"] == 1][0]
            a4_trace.append(dict(case=case, model=model, tasks=tasks, off_wall_ms=round(off, 2),
                                 on_wall_ms=round(on, 2), increase_fraction=round(on / off - 1, 4),
                                 off_peak_rss_mib=round(off_rss / 2**20, 2),
                                 on_peak_rss_mib=round(on_rss / 2**20, 2), trace_bytes=trace_bytes))
    write_csv(report / "a4_trace_overhead.csv", a4_trace)

    # ---- Plots --------------------------------------------------------------
    labels = {"empty": "空输入", "single": "单任务", "zero_delay": "零延迟", "short": "短任务",
              "long": "长任务", "mixed": "混合", "skew": "长短偏斜", "slow_sink": "慢输出",
              "sparse_wait": "稀疏等待"}
    grouped_bar_chart(
        report / "a1_utilization.svg", [labels[row["case"]] for row in a1],
        {"忙周期/T": [round(row["utilization"], 4) for row in a1],
         "无输入/T": [round(row["idle_no_input_cycles"] / row["cycles"], 4) for row in a1],
         "结果等待/T": [round(row["result_wait_cycles"] / row["cycles"], 4) for row in a1]},
        ylabel="占 T 比例", title="作业1 stage1 Compute 时间构成（默认 D=2, P=1）")
    line_chart(report / "a2_depth_throughput.svg", [row["depth"] for row in a2_depth],
               {"吞吐(任务/周期)": [row["throughput"] for row in a2_depth]},
               xlabel="FIFO 深度 D", ylabel="吞吐", title="作业2 容量扫描：吞吐 vs D（mixed, P=1）")
    line_chart(report / "a2_depth_blocked.svg", [row["depth"] for row in a2_depth],
               {"阻塞/有效 S/V": [row["blocked_over_valid"] for row in a2_depth],
                "阻塞/全程 S/T": [row["blocked_over_total"] for row in a2_depth]},
               xlabel="FIFO 深度 D", ylabel="阻塞比例",
               title="作业2 容量扫描：阻塞率 vs D", dashed={"阻塞/全程 S/T"})
    line_chart(report / "a2_period_throughput.svg", [row["period"] for row in a2_period],
               {"吞吐(任务/周期)": [row["throughput"] for row in a2_period]},
               xlabel="输出节拍 P", ylabel="吞吐", title="作业2 输出节拍扫描：吞吐 vs P（short, D=2）")
    line_chart(report / "a2_period_blocked.svg", [row["period"] for row in a2_period],
               {"阻塞/有效 S/V": [row["blocked_over_valid"] for row in a2_period],
                "阻塞/全程 S/T": [row["blocked_over_total"] for row in a2_period]},
               xlabel="输出节拍 P", ylabel="阻塞比例",
               title="作业2 输出节拍扫描：阻塞率 vs P", dashed={"阻塞/全程 S/T"})
    grouped_bar_chart(
        report / "a3_cycles.svg", [labels[row["case"]] for row in a3],
        {"stage2 单实例": [row["t_single"] for row in a3], "stage3 双实例轮转": [row["t_dual"] for row in a3],
         "stage3_window 择闲窗口": [row["t_window"] for row in a3]},
        ylabel="总周期 T（对数轴）", title="作业3 单/双实例总周期对比", log_y=True)
    line_chart(report / "a3_window_throughput.svg", [row["window"] for row in a3_window],
               {"吞吐(任务/周期)": [row["throughput"] for row in a3_window]},
               xlabel="窗口容量 W", ylabel="吞吐", title="作业3 窗口扫描：吞吐 vs W（skew）")
    line_chart(report / "a3_window_storage.svg", [row["window"] for row in a3_window],
               {"窗口位开销(bits)": [row["window_bits"] for row in a3_window]},
               xlabel="窗口容量 W", ylabel="位", title="作业3 窗口扫描：保序存储代价 96W+W")
    grouped_bar_chart(
        report / "a4_wall_time.svg", [labels[row["case"]] for row in a4],
        {"时钟模型(ms)": [row["clock_wall_ms"] for row in a4], "事件模型(ms)": [row["event_wall_ms"] for row in a4]},
        ylabel="主机耗时 ms（对数轴）", title="作业4 主机进程耗时：时钟 vs 事件模型",
        log_y=True, errors={"时钟模型(ms)": [(row["clock_q1"], row["clock_q3"]) for row in a4],
                            "事件模型(ms)": [(row["event_q1"], row["event_q3"]) for row in a4]})


if __name__ == "__main__":
    main()
