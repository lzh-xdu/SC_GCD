"""Black-box checks: numerical oracle, event timing and bounded-resource accounting."""
import csv
import json
import math
from pathlib import Path
import random
import subprocess
import sys

test_exe = str(Path(sys.argv[1]).resolve())
test_root = Path(sys.argv[2]).resolve()
test_root.mkdir(parents=True, exist_ok=True)
test_summary = []


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
    require(all(test_line.isascii() and test_line.isdecimal() for test_line in test_output.read_text().splitlines()),
            f"{test_name}: diagnostic/statistics leaked into functional output")
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
    test_summary.append({"case": test_name, **test_metrics})
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

test_cases = Path(__file__).parent / "cases"
for test_index, test_case in enumerate(json.loads((test_cases / "manifest.json").read_text())):
    test_name = test_case["name"]
    test_tasks = [tuple(map(int, test_line.split()))
                  for test_line in (test_cases / f"{test_name}.txt").read_text().splitlines()]
    require(len(test_tasks) == test_case["tasks"], "fixture count mismatch")
    test_events, test_metrics = run_case(test_name, test_tasks, test_depth=1 if test_index % 2 == 0 else 4)
    require((test_root / test_name / "output.txt").read_text() ==
            (test_cases / f"{test_name}.expected.txt").read_text(), "fixture expected output mismatch")
    if test_name == "fibonacci_consecutive":
        require(all(latency(*test_task) >= 40 for test_task in test_tasks), "not a long-latency workload")
        require(all(math.gcd(*test_task) == 1 for test_task in test_tasks), "adjacent Fibonacci GCD")
        require(test_metrics["parser_to_transform_peak"] == test_metrics["parser_to_transform_capacity"],
                "long tasks did not fill upstream FIFO")
    if test_name == "short_baseline":
        test_short_metrics = test_metrics
    if test_name == "short_with_long":
        # Compare at the same capacity and sink rate, including the added tasks.
        test_mixed_events, test_mixed_metrics = run_case("mixed_depth1", test_tasks, test_depth=1)
        test_short_events, test_short_metrics = run_case("short_depth1", [(test_i, test_i) for test_i in range(1, 41)],
                                                       test_depth=1)
        require(test_mixed_metrics["cycles"] > test_short_metrics["cycles"], "mixed workload did not take longer")
        for test_id in (10, 21, 32):
            require(test_mixed_events["compute_accept"][test_id + 1]["cycle"] >
                    test_mixed_events["compute_complete"][test_id]["cycle"], "short task overtook long task")

# Instrumentation must not change results or simulated performance.
test_folder = test_root / "without_events"
test_folder.mkdir(exist_ok=True)
test_result = subprocess.run([test_exe, str(test_root / "basic/input.txt"), str(test_folder / "output.txt"),
                              str(test_folder / "stats.csv"), "2", "-"],
                             capture_output=True, text=True, timeout=10)
(test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
require(test_result.returncode == 0, "run without tracing failed")
for test_file in ("output.txt", "stats.csv"):
    require((test_folder / test_file).read_bytes() == (test_root / "basic" / test_file).read_bytes(),
            "tracing changed output or simulated statistics")

test_invalid = ["1\n", "a 2\n", "1 2 3\n", "2147483648 1\n", "\n", "1 2.5\n",
                "-2147483649 1\n", "1 2147483648\n", "1 -2147483649\n",
                "99999999999999999999999999999 1\n", "1e3 2\n", "1 0x10\n", "--1 2\n", "3 3\n1\n"]
for test_i, test_text in enumerate(test_invalid):
    test_folder = test_root / f"invalid_{test_i}"
    test_folder.mkdir(exist_ok=True)
    (test_folder / "input.txt").write_text(test_text, encoding="utf-8")
    test_result = subprocess.run([test_exe, str(test_folder/"input.txt"), str(test_folder/"output.txt"), str(test_folder/"stats.csv")],
                            capture_output=True, text=True, timeout=10)
    (test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    require(test_result.returncode != 0 and "invalid input line" in test_result.stderr, "invalid input not diagnosed")
    require("PASS:" not in test_result.stderr, "invalid input reported success")

# Reject path aliasing before opening/truncating any data file.
test_source = test_root / "single/input.txt"
test_original = test_source.read_bytes()
for test_output_path, test_stats_path in [(test_source, test_root / "collision.csv"),
                                          (test_root / "collision.txt", test_root / "collision.txt")]:
    test_result = subprocess.run([test_exe, str(test_source), str(test_output_path), str(test_stats_path)],
                                 capture_output=True, text=True, timeout=10)
    require(test_result.returncode != 0 and "distinct files" in test_result.stderr, "path collision accepted")
    require(test_source.read_bytes() == test_original, "input truncated by path collision")
test_result = subprocess.run([test_exe, str(test_root/"single/input.txt"), str(test_root/"timeout.txt"), str(test_root/"timeout.csv"),
                         "2", "-", "1", "5"], capture_output=True, text=True, timeout=10)
require(test_result.returncode != 0 and "cycle limit" in test_result.stderr, "timeout not detected")
with (test_root / "summary.csv").open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_summary[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_summary)
print(f"PASS {len(test_invalid)} invalid inputs, output separation, path collisions and simulation timeout")
print(f"PASS all stage1 checks: {len(test_summary)} valid scenarios, "
      f"{int(sum(test_row['tasks'] for test_row in test_summary))} tasks")
