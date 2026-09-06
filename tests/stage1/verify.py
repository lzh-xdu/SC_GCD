"""Black-box checks: numerical oracle, event timing and bounded-resource accounting."""
import csv
import math
from pathlib import Path
import random
import subprocess
import sys

test_exe = str(Path(sys.argv[1]).resolve())
test_root = Path(sys.argv[2]).resolve()
test_root.mkdir(parents=True, exist_ok=True)


def require(test_condition, test_message):
    if not test_condition:
        raise AssertionError(test_message)


def latency(test_a, test_b):
    test_a, test_b = sorted((abs(test_a), abs(test_b)), reverse=True)
    test_total = 0
    while test_b:
        test_total += max(1, test_a.bit_length() - test_b.bit_length() + 1)
        test_a, test_b = test_b, test_a % test_b
    return test_total


def run_case(test_name, test_tasks, test_depth=2, test_period=1):
    test_folder = test_root / test_name
    test_folder.mkdir(exist_ok=True)
    test_source, test_output, test_stats, test_events = [test_folder / test_p for test_p in ("input.txt", "output.txt", "stats.csv", "events.csv")]
    test_source.write_text("".join(f"{test_a} {test_b}\n" for test_a, test_b in test_tasks), encoding="utf-8")
    test_command = [test_exe, str(test_source), str(test_output), str(test_stats), str(test_depth), str(test_events), str(test_period)]
    test_result = subprocess.run(test_command, capture_output=True, text=True, timeout=15)
    (test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    require(test_result.returncode == 0, f"{test_name}: {test_result.stdout} {test_result.stderr}")
    test_expected = "".join(f"{math.gcd(test_a,test_b)}\n" for test_a, test_b in test_tasks)
    require(test_output.read_text() == test_expected, f"{test_name}: functional output mismatch")
    test_metrics = {test_row["metric"]: float(test_row["value"]) for test_row in csv.DictReader(test_stats.open())}
    test_rows = [{test_k: test_v if test_k == "event" else int(test_v) for test_k, test_v in test_row.items()}
            for test_row in csv.DictReader(test_events.open())]
    test_by_event = {test_name: {} for test_name in ("parser_send", "transform_accept", "transform_emit",
                                    "compute_accept", "compute_complete", "compute_emit", "output")}
    for test_row in test_rows:
        test_event = test_by_event[test_row["event"]]
        require(test_row["id"] not in test_event, "duplicated event for same task")
        test_event[test_row["id"]] = test_row
    for test_event in test_by_event.values():
        require(sorted(test_event) == list(range(len(test_tasks))), "missing or extra task event")
        require(list(test_event) == list(range(len(test_tasks))), "event order changed")
    for test_i, (test_a, test_b) in enumerate(test_tasks):
        test_t = {test_event: test_entries[test_i]["cycle"] for test_event, test_entries in test_by_event.items()}
        require(test_t["transform_accept"] > test_t["parser_send"], "same-edge FIFO bypass")
        require(test_t["transform_emit"] - test_t["transform_accept"] >= 2, "transform too early")
        require(test_t["compute_accept"] > test_t["transform_emit"], "same-edge compute acceptance")
        require(test_t["compute_complete"] - test_t["compute_accept"] == latency(test_a, test_b), "wrong compute delay")
        require(test_t["compute_emit"] >= test_t["compute_complete"], "result emitted before completion")
        require(test_t["output"] > test_t["compute_emit"], "same-edge result consumption")
        require(test_t["output"] % test_period == 0, "output rate violated")
        test_accepted = test_by_event["compute_accept"][test_i]
        require((test_accepted["a"], test_accepted["b"]) == tuple(sorted((abs(test_a), abs(test_b)), reverse=True)),
                "absolute-value or ordering error")
        if test_i:
            require(test_t["compute_accept"] > test_by_event["compute_emit"][test_i-1]["cycle"],
                    "accepted next task on completion/delivery edge")
    test_cycles = int(test_metrics["cycles"])
    require(test_metrics["tasks"] == len(test_tasks), "task metric")
    require(test_metrics["compute_busy_cycles"] == sum(latency(test_a,test_b) for test_a,test_b in test_tasks), "busy metric")
    require(abs(test_metrics["throughput_tasks_per_cycle"] - len(test_tasks)/test_cycles) < 1e-10, "throughput metric")
    require(abs(test_metrics["compute_utilization"] - test_metrics["compute_busy_cycles"]/test_cycles) < 1e-10,
            "utilization metric")
    # Reconstruct occupancy independently from interface events, per edge.
    for test_prefix, test_put, test_take, test_capacity in [
        ("parser_to_transform", "parser_send", "transform_accept", test_depth),
        ("transform_to_compute", "transform_emit", "compute_accept", test_depth),
        ("compute_to_output", "compute_emit", "output", test_depth),
        ("transform_registers", "transform_accept", "transform_emit", 2),
    ]:
        test_puts, test_takes = {}, {}
        for test_row in test_by_event[test_put].values(): test_puts[test_row["cycle"]] = test_puts.get(test_row["cycle"], 0) + 1
        for test_row in test_by_event[test_take].values(): test_takes[test_row["cycle"]] = test_takes.get(test_row["cycle"], 0) + 1
        test_occupancy = test_area = test_peak = 0
        for test_c in range(1, test_cycles + 1):
            require(test_puts.get(test_c, 0) <= 1 and test_takes.get(test_c, 0) <= 1, "rate exceeds one task per edge")
            if test_prefix != "transform_registers":
                require(test_takes.get(test_c, 0) <= test_occupancy, "read from initially empty FIFO")
                require(test_puts.get(test_c, 0) <= test_capacity - test_occupancy, "write to initially full FIFO")
            test_occupancy += test_puts.get(test_c, 0) - test_takes.get(test_c, 0)
            require(0 <= test_occupancy <= test_capacity, "capacity violated")
            test_area += test_occupancy
            test_peak = max(test_peak, test_occupancy)
        require(test_occupancy == 0, "system not drained")
        require(test_metrics[test_prefix + "_peak"] == test_peak, "peak metric mismatch")
        require(abs(test_metrics[test_prefix + "_average"] - test_area/test_cycles) < 1e-9, "average metric mismatch")
    print(f"PASS {test_name}: {len(test_tasks)} tasks, {test_cycles} cycles")
    return test_by_event, test_metrics


test_single, test_metrics = run_case("single", [(48,18)])
require(test_metrics["cycles"] == 12, "single task end-to-end timeline")
require([test_single[test_event][0]["cycle"] for test_event in test_single] == [1,2,4,5,11,11,12], "golden timeline")
test_basic = [tuple(map(int, test_line.split())) for test_line in (Path(__file__).parent / "basic.txt").read_text().splitlines()]
run_case("basic", test_basic)
require((test_root / "basic/output.txt").read_text() == (Path(__file__).parent / "basic.expected.txt").read_text(),
        "published basic expected file")
run_case("empty", [])
run_case("boundaries", [(0,0),(0,7),(-7,0),(-2147483648,0),(-2147483648,-2147483648),
                         (2147483647,-2147483648),(-24,36),(30,30),(30,30)], test_depth=1)
test_pipeline, test__ = run_case("pipeline", [(test_i,0) for test_i in range(1,21)], test_depth=64)
require([test_r["cycle"] for test_r in test_pipeline["transform_accept"].values()] == list(range(2,22)), "pipeline input rate")
require([test_r["cycle"] for test_r in test_pipeline["transform_emit"].values()] == list(range(4,24)), "pipeline output rate/delay")
test__, test_slow = run_case("slow_sink", [(30,30)] * 24, test_depth=1, test_period=13)
require(test_slow["compute_result_wait_cycles"] > 0, "backpressure scenario failed to cause waiting")
test_rng = random.Random(20260906)
test_tasks = [(test_rng.randint(-2**31, 2**31-1), test_rng.randint(-2**31, 2**31-1)) for test__ in range(250)]
run_case("random", test_tasks, test_depth=1)
run_case("random_buffered", test_tasks, test_depth=4, test_period=3)

for test_i, test_text in enumerate(["1\n", "a 2\n", "1 2 3\n", "2147483648 1\n", "\n", "1 2.5\n"]):
    test_folder = test_root / f"invalid_{test_i}"
    test_folder.mkdir(exist_ok=True)
    (test_folder / "input.txt").write_text(test_text)
    test_result = subprocess.run([test_exe, str(test_folder/"input.txt"), str(test_folder/"output.txt"), str(test_folder/"stats.csv")],
                            capture_output=True, text=True, timeout=10)
    require(test_result.returncode != 0, "invalid input accepted")
test_result = subprocess.run([test_exe, str(test_root/"single/input.txt"), str(test_root/"timeout.txt"), str(test_root/"timeout.csv"),
                         "2", "-", "1", "5"], capture_output=True, text=True, timeout=10)
require(test_result.returncode != 0 and "cycle limit" in test_result.stderr, "timeout not detected")
print("PASS invalid inputs and simulation timeout; all stage1 checks passed")
