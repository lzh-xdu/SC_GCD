"""Black-box checks: numerical oracle, event timing and bounded-resource accounting."""
import csv
import math
from pathlib import Path
import random
import subprocess
import sys

exe = str(Path(sys.argv[1]).resolve())
root = Path(sys.argv[2]).resolve()
root.mkdir(parents=True, exist_ok=True)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def latency(a, b):
    a, b = sorted((abs(a), abs(b)), reverse=True)
    total = 0
    while b:
        total += max(1, a.bit_length() - b.bit_length() + 1)
        a, b = b, a % b
    return total


def run_case(name, tasks, depth=2, period=1):
    folder = root / name
    folder.mkdir(exist_ok=True)
    source, output, stats, events = [folder / p for p in ("input.txt", "output.txt", "stats.csv", "events.csv")]
    source.write_text("".join(f"{a} {b}\n" for a, b in tasks), encoding="utf-8")
    command = [exe, str(source), str(output), str(stats), str(depth), str(events), str(period)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=15)
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    require(result.returncode == 0, f"{name}: {result.stdout} {result.stderr}")
    expected = "".join(f"{math.gcd(a,b)}\n" for a, b in tasks)
    require(output.read_text() == expected, f"{name}: functional output mismatch")
    metrics = {row["metric"]: float(row["value"]) for row in csv.DictReader(stats.open())}
    rows = [{k: v if k == "event" else int(v) for k, v in row.items()}
            for row in csv.DictReader(events.open())]
    by_event = {name: {} for name in ("parser_send", "transform_accept", "transform_emit",
                                    "compute_accept", "compute_complete", "compute_emit", "output")}
    for row in rows:
        event = by_event[row["event"]]
        require(row["id"] not in event, "duplicated event for same task")
        event[row["id"]] = row
    for event in by_event.values():
        require(sorted(event) == list(range(len(tasks))), "missing or extra task event")
        require(list(event) == list(range(len(tasks))), "event order changed")
    for i, (a, b) in enumerate(tasks):
        t = {event: entries[i]["cycle"] for event, entries in by_event.items()}
        require(t["transform_accept"] > t["parser_send"], "same-edge FIFO bypass")
        require(t["transform_emit"] - t["transform_accept"] >= 2, "transform too early")
        require(t["compute_accept"] > t["transform_emit"], "same-edge compute acceptance")
        require(t["compute_complete"] - t["compute_accept"] == latency(a, b), "wrong compute delay")
        require(t["compute_emit"] >= t["compute_complete"], "result emitted before completion")
        require(t["output"] > t["compute_emit"], "same-edge result consumption")
        require(t["output"] % period == 0, "output rate violated")
        accepted = by_event["compute_accept"][i]
        require((accepted["a"], accepted["b"]) == tuple(sorted((abs(a), abs(b)), reverse=True)),
                "absolute-value or ordering error")
        if i:
            require(t["compute_accept"] > by_event["compute_emit"][i-1]["cycle"],
                    "accepted next task on completion/delivery edge")
    cycles = int(metrics["cycles"])
    require(metrics["tasks"] == len(tasks), "task metric")
    require(metrics["compute_busy_cycles"] == sum(latency(a,b) for a,b in tasks), "busy metric")
    require(abs(metrics["throughput_tasks_per_cycle"] - len(tasks)/cycles) < 1e-10, "throughput metric")
    require(abs(metrics["compute_utilization"] - metrics["compute_busy_cycles"]/cycles) < 1e-10,
            "utilization metric")
    # Reconstruct occupancy independently from interface events, per edge.
    for prefix, put, take, capacity in [
        ("parser_to_transform", "parser_send", "transform_accept", depth),
        ("transform_to_compute", "transform_emit", "compute_accept", depth),
        ("compute_to_output", "compute_emit", "output", depth),
        ("transform_registers", "transform_accept", "transform_emit", 2),
    ]:
        puts, takes = {}, {}
        for row in by_event[put].values(): puts[row["cycle"]] = puts.get(row["cycle"], 0) + 1
        for row in by_event[take].values(): takes[row["cycle"]] = takes.get(row["cycle"], 0) + 1
        occupancy = area = peak = 0
        for c in range(1, cycles + 1):
            require(puts.get(c, 0) <= 1 and takes.get(c, 0) <= 1, "rate exceeds one task per edge")
            if prefix != "transform_registers":
                require(takes.get(c, 0) <= occupancy, "read from initially empty FIFO")
                require(puts.get(c, 0) <= capacity - occupancy, "write to initially full FIFO")
            occupancy += puts.get(c, 0) - takes.get(c, 0)
            require(0 <= occupancy <= capacity, "capacity violated")
            area += occupancy
            peak = max(peak, occupancy)
        require(occupancy == 0, "system not drained")
        require(metrics[prefix + "_peak"] == peak, "peak metric mismatch")
        require(abs(metrics[prefix + "_average"] - area/cycles) < 1e-9, "average metric mismatch")
    print(f"PASS {name}: {len(tasks)} tasks, {cycles} cycles")
    return by_event, metrics


single, metrics = run_case("single", [(48,18)])
require(metrics["cycles"] == 12, "single task end-to-end timeline")
require([single[event][0]["cycle"] for event in single] == [1,2,4,5,11,11,12], "golden timeline")
basic = [tuple(map(int, line.split())) for line in (Path(__file__).parent / "basic.txt").read_text().splitlines()]
run_case("basic", basic)
require((root / "basic/output.txt").read_text() == (Path(__file__).parent / "basic.expected.txt").read_text(),
        "published basic expected file")
run_case("empty", [])
run_case("boundaries", [(0,0),(0,7),(-7,0),(-2147483648,0),(-2147483648,-2147483648),
                         (2147483647,-2147483648),(-24,36),(30,30),(30,30)], depth=1)
pipeline, _ = run_case("pipeline", [(i,0) for i in range(1,21)], depth=64)
require([r["cycle"] for r in pipeline["transform_accept"].values()] == list(range(2,22)), "pipeline input rate")
require([r["cycle"] for r in pipeline["transform_emit"].values()] == list(range(4,24)), "pipeline output rate/delay")
_, slow = run_case("slow_sink", [(30,30)] * 24, depth=1, period=13)
require(slow["compute_result_wait_cycles"] > 0, "backpressure scenario failed to cause waiting")
rng = random.Random(20260906)
tasks = [(rng.randint(-2**31, 2**31-1), rng.randint(-2**31, 2**31-1)) for _ in range(250)]
run_case("random", tasks, depth=1)
run_case("random_buffered", tasks, depth=4, period=3)

for i, text in enumerate(["1\n", "a 2\n", "1 2 3\n", "2147483648 1\n", "\n", "1 2.5\n"]):
    folder = root / f"invalid_{i}"
    folder.mkdir(exist_ok=True)
    (folder / "input.txt").write_text(text)
    result = subprocess.run([exe, str(folder/"input.txt"), str(folder/"output.txt"), str(folder/"stats.csv")],
                            capture_output=True, text=True, timeout=10)
    require(result.returncode != 0, "invalid input accepted")
result = subprocess.run([exe, str(root/"single/input.txt"), str(root/"timeout.txt"), str(root/"timeout.csv"),
                         "2", "-", "1", "5"], capture_output=True, text=True, timeout=10)
require(result.returncode != 0 and "cycle limit" in result.stderr, "timeout not detected")
print("PASS invalid inputs and simulation timeout; all stage1 checks passed")
