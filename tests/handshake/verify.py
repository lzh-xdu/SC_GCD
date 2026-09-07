"""Independent arithmetic, edge timing, handshake retention and resource checks."""
import csv
import json
import math
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from task_statistics import check_statistics

TEST_EXE = str(Path(sys.argv[1]).resolve())
TEST_ROOT = Path(sys.argv[2]).resolve()
TEST_UNITS = int(sys.argv[3])
TEST_WINDOW = int(sys.argv[4]) if len(sys.argv) > 4 else 0
TEST_CASES = Path(__file__).parents[1] / "stage1/cases"
TEST_ROOT.mkdir(parents=True, exist_ok=True)
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


def check_links(test_rows, test_events, test_metrics):
    test_links = [test_row for test_row in test_rows if test_row["event"] == "link"]
    test_last = None
    test_transfers = []
    test_window_ready_blocked = 0
    test_random = int(test_metrics.get("random_seed", 1))
    for test_row in test_links:
        test_ready = [not any(
            test_events["compute_unit"][test_id]["a"] == test_unit and
            test_start["cycle"] < test_row["cycle"] <= test_events["compute_emit"][test_id]["cycle"]
            for test_id, test_start in test_events["compute_accept"].items()) for test_unit in range(TEST_UNITS)]
        if TEST_WINDOW:
            test_base = sum(test_retire["cycle"] < test_row["cycle"]
                            for test_retire in test_events["reorder_emit"].values())
            test_credit = test_row["id"] - test_base < test_metrics["window_capacity"]
            require(bool(test_row["value"]) == (test_credit and any(test_ready)), "window/ready capacity mismatch")
            test_window_ready_blocked += int(not test_credit and any(test_ready))
            if test_row["value"]:
                test_selected = test_events["compute_unit"][test_row["id"]]["a"]
                if all(test_ready):
                    require(test_selected == test_random % 2, "seeded random tie mismatch")
                    test_random ^= (test_random << 13) & 0xffffffff
                    test_random ^= test_random >> 17
                    test_random ^= (test_random << 5) & 0xffffffff
                else:
                    require(test_ready[test_selected], "did not choose available compute")
        else:
            require(bool(test_row["value"]) == test_ready[test_row["id"] % TEST_UNITS], "ready capacity mismatch")
        if test_last is not None and test_last["value"] == 0:
            require(test_row["cycle"] == test_last["cycle"] + 1, "valid withdrawn while stalled")
            require(all(test_row[test_key] == test_last[test_key] for test_key in ("id", "a", "b")),
                    "payload overwritten while stalled")
        if test_row["value"]:
            test_transfers.append((test_row["id"], test_row["cycle"]))
        test_last = test_row
    require(test_last is None or test_last["value"] == 1, "last transfer left stalled")
    require(test_transfers == [(test_id, test_row["cycle"])
                              for test_id, test_row in test_events["compute_accept"].items()], "handshake mismatch")
    if TEST_WINDOW:
        require(test_window_ready_blocked == test_metrics["window_blocked_with_ready_cycles"], "ready window blockage")
    require(len(test_links) == test_metrics["handshake_valid_cycles"], "valid metric")
    test_blocked = sum(test_row["value"] == 0 for test_row in test_links)
    require(test_blocked == test_metrics["handshake_blocked_cycles"], "blocked metric")
    require(abs(test_metrics["handshake_blocked_fraction"] - test_blocked / max(1, len(test_links))) < 1e-10,
            "blocked fraction")


def check_timing(test_tasks, test_events, test_metrics, test_period):
    test_last_emit = [-1] * TEST_UNITS
    test_busy = [0] * TEST_UNITS
    for test_id, (test_a, test_b) in enumerate(test_tasks):
        test_time = {test_name: test_entries[test_id]["cycle"] for test_name, test_entries in test_events.items()}
        test_unit = test_events["compute_unit"][test_id]["a"]
        require(0 <= test_unit < TEST_UNITS, "invalid compute unit")
        if not TEST_WINDOW:
            require(test_unit == test_id % TEST_UNITS, "round-robin assignment violated")
        test_start = test_time["compute_accept"]
        require(test_start > test_last_emit[test_unit], "accepted on completion/delivery edge")
        test_last_emit[test_unit] = test_time["compute_emit"]
        test_delay = latency(test_a, test_b)
        test_busy[test_unit] += test_delay
        require(test_time["transform_accept"] > test_time["parser_send"], "FIFO same-edge bypass")
        require(test_start - test_time["transform_accept"] >= 2, "transform too early")
        require(test_start == test_time["transform_emit"], "transfer edge mismatch")
        require(test_time["compute_complete"] == test_start + test_delay, "compute latency mismatch")
        require(test_time["compute_emit"] >= test_time["compute_complete"], "premature result")
        require(test_time["output"] > test_time["compute_emit"], "result FIFO bypass")
        require(test_time["output"] % test_period == 0, "sink period mismatch")
        test_accept = test_events["compute_accept"][test_id]
        require((test_accept["a"], test_accept["b"]) == tuple(sorted((abs(test_a), abs(test_b)), reverse=True)),
                "transform value error")
        if TEST_UNITS == 2:
            require(test_time["reorder_emit"] > test_time["compute_emit"], "collector FIFO bypass")
            require(test_time["output"] > test_time["reorder_emit"], "output FIFO bypass")
        if TEST_WINDOW:
            require(test_time["window_store"] > test_time["compute_emit"], "window FIFO bypass")
            require(test_time["reorder_emit"] > test_time["window_store"], "same-edge window bypass")
    test_cycles = test_metrics["cycles"]
    require(sum(test_busy) == test_metrics["compute_busy_cycles"], "busy cycles mismatch")
    require(sum(test_events["compute_emit"][test_id]["cycle"] - test_row["cycle"]
                for test_id, test_row in test_events["compute_complete"].items()) ==
            test_metrics["compute_result_wait_cycles"], "result wait count mismatch")
    require(abs(test_metrics["compute_utilization"] - sum(test_busy) / (TEST_UNITS * test_cycles)) < 1e-10,
            "aggregate utilization mismatch")
    require(test_metrics["tasks"] == len(test_tasks), "task count mismatch")
    require(abs(test_metrics["throughput_tasks_per_cycle"] - len(test_tasks) / test_cycles) < 1e-10, "throughput")
    for test_unit in range(TEST_UNITS):
        if TEST_UNITS == 2:
            require(test_metrics[f"compute{test_unit}_busy_cycles"] == test_busy[test_unit], "unit busy count")
            require(abs(test_metrics[f"compute{test_unit}_utilization"] - test_busy[test_unit] / test_cycles) < 1e-10,
                    "unit utilization")


def check_occupancy(test_events, test_metrics):
    test_resources = [("parser_to_transform", "parser_send", "transform_accept", None),
                      ("transform_registers", "transform_accept", "transform_emit", None)]
    if TEST_UNITS == 1:
        test_resources.append(("compute_to_output", "compute_emit", "output", None))
    else:
        test_resources += [(f"compute{test_unit}_results", "compute_emit",
                            "window_store" if TEST_WINDOW else "reorder_emit", test_unit)
                           for test_unit in range(TEST_UNITS)]
        test_resources.append(("collector_to_output", "reorder_emit", "output", None))
    for test_name, test_put, test_take, test_unit in test_resources:
        test_puts, test_takes = {}, {}
        for test_event, test_counts in ((test_put, test_puts), (test_take, test_takes)):
            for test_id, test_row in test_events[test_event].items():
                if test_unit is None or test_events["compute_unit"][test_id]["a"] == test_unit:
                    test_counts[test_row["cycle"]] = test_counts.get(test_row["cycle"], 0) + 1
        test_count = test_area = test_peak = 0
        test_capacity = test_metrics[test_name + "_capacity"]
        for test_cycle in range(1, int(test_metrics["cycles"]) + 1):
            test_add = test_puts.get(test_cycle, 0)
            test_remove = test_takes.get(test_cycle, 0)
            require(test_add <= 1 and test_remove <= 1, "per-port rate exceeded")
            if test_name != "transform_registers":
                require(test_remove <= test_count and test_add <= test_capacity - test_count, "FIFO boundary error")
            test_count += test_add - test_remove
            require(0 <= test_count <= test_capacity, "capacity exceeded")
            test_area += test_count
            test_peak = max(test_peak, test_count)
        require(test_count == 0, "not drained")
        require(test_peak == test_metrics[test_name + "_peak"], "occupancy peak mismatch")
        require(abs(test_area / test_metrics["cycles"] - test_metrics[test_name + "_average"]) < 1e-9, "occupancy mean")


def check_window(test_events, test_metrics):
    test_cycles = int(test_metrics["cycles"])
    for test_name, test_enter in (("results", "window_store"), ("reserved", "compute_accept")):
        test_count = test_area = test_peak = 0
        test_puts = {test_row["cycle"] for test_row in test_events[test_enter].values()}
        test_takes = {test_row["cycle"] for test_row in test_events["reorder_emit"].values()}
        require(len(test_puts) == len(test_events[test_enter]), "window port rate exceeded")
        require(len(test_takes) == len(test_events["reorder_emit"]), "retire rate exceeded")
        for test_cycle in range(1, test_cycles + 1):
            test_count += (test_cycle in test_puts) - (test_cycle in test_takes)
            require(0 <= test_count <= test_metrics["window_capacity"], "window capacity exceeded")
            test_area += test_count
            test_peak = max(test_peak, test_count)
        require(test_count == 0, "window not drained")
        require(test_metrics[f"window_{test_name}_peak"] == test_peak, "window peak mismatch")
        require(abs(test_metrics[f"window_{test_name}_average"] - test_area / test_cycles) < 1e-9, "window average")


def run_case(test_name, test_tasks, test_depth=2, test_period=1, test_result_depth=2,
             test_window=TEST_WINDOW, test_seed=1):
    test_folder = TEST_ROOT / test_name
    test_folder.mkdir(exist_ok=True)
    (test_folder / "input.txt").write_text("".join(f"{test_a} {test_b}\n" for test_a, test_b in test_tasks), encoding="utf-8")
    test_command = [TEST_EXE, *(str(test_folder / test_file) for test_file in ("input.txt", "output.txt", "stats.csv")),
                    str(test_depth), str(test_folder / "events.csv"), str(test_period), "1000000"]
    if TEST_UNITS == 2:
        test_command.append(str(test_result_depth))
    if TEST_WINDOW:
        test_command.extend((str(test_window), str(test_seed)))
    test_result = subprocess.run(test_command, capture_output=True, text=True, timeout=15)
    (test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    require(test_result.returncode == 0, f"{test_name}: {test_result.stderr}")
    require((test_folder / "output.txt").read_text() == "".join(f"{math.gcd(*test_task)}\n" for test_task in test_tasks),
            f"{test_name}: wrong functional output")
    test_metrics = {test_row["metric"]: float(test_row["value"])
                    for test_row in csv.DictReader((test_folder / "stats.csv").open())}
    test_rows = [{test_key: test_value if test_key == "event" else int(test_value)
                 for test_key, test_value in test_row.items()}
                for test_row in csv.DictReader((test_folder / "events.csv").open())]
    test_events = {test_name: {} for test_name in ("parser_send", "transform_accept", "transform_emit", "compute_accept",
                                                 "compute_unit", "compute_complete", "compute_emit", "output")}
    if TEST_UNITS == 2:
        test_events["reorder_emit"] = {}
    if TEST_WINDOW:
        test_events["window_store"] = {}
    for test_row in test_rows:
        if test_row["event"] in test_events:
            test_entries = test_events[test_row["event"]]
            require(test_row["id"] not in test_entries, "duplicate task event")
            test_entries[test_row["id"]] = test_row
    for test_name, test_entries in test_events.items():
        require(sorted(test_entries) == list(range(len(test_tasks))), "missing task events")
        if test_name not in ("compute_complete", "compute_emit", "window_store"):
            require(list(test_entries) == list(range(len(test_tasks))), "task order mismatch")
    check_links(test_rows, test_events, test_metrics)
    check_timing(test_tasks, test_events, test_metrics, test_period)
    check_occupancy(test_events, test_metrics)
    if TEST_UNITS == 2 and not TEST_WINDOW:
        for test_event, test_metric in (("dispatch_idle_other", "dispatch_idle_other_blocked_cycles"),
                                       ("reorder_wait", "reorder_wait_cycles"),
                                       ("collector_blocked", "collector_output_blocked_cycles")):
            require(sum(test_row["event"] == test_event for test_row in test_rows) == test_metrics[test_metric],
                    "control counter mismatch")
    if TEST_WINDOW:
        check_window(test_events, test_metrics)
        require(test_metrics["handshake_blocked_cycles"] ==
                test_metrics["window_blocked_cycles"] + test_metrics["engines_blocked_cycles"], "blocked split")
    check_statistics(test_rows, test_metrics, TEST_UNITS)
    test_summary.append({"case": test_folder.name, **test_metrics})
    print(f"PASS {test_folder.name}: {len(test_tasks)} tasks, {int(test_metrics['cycles'])} cycles, "
          f"{int(test_metrics['handshake_blocked_cycles'])} blocked")
    return test_events, test_metrics


test_events, test_metrics = run_case("single", [(48, 18)])
require(test_events["compute_accept"][0]["cycle"] == 4, "golden accept edge")
require(test_events["compute_complete"][0]["cycle"] == 10, "golden complete edge")
require(test_metrics["cycles"] == (13 if TEST_WINDOW else 11 if TEST_UNITS == 1 else 12), "golden drain edge")
run_case("empty", [])
test_basic = [tuple(map(int, test_line.split()))
              for test_line in (TEST_CASES.parent / "basic.txt").read_text().splitlines()]
run_case("basic", test_basic)
run_case("zero_pipeline", [(test_i, 0) for test_i in range(24)], test_depth=1)
test_events, test_metrics = run_case("slow_sink", [(30, 30)] * 40, test_depth=1, test_period=13, test_result_depth=1)
if TEST_WINDOW:
    require(test_metrics["window_blocked_cycles"] > 0, "no window backpressure exercised")
else:
    require(test_metrics["compute_result_wait_cycles"] > 0, "no result backpressure exercised")
require(test_metrics["handshake_blocked_cycles"] > 0, "no handshake stall exercised")
for test_index, test_case in enumerate(json.loads((TEST_CASES / "manifest.json").read_text())):
    test_tasks = [tuple(map(int, test_line.split()))
                  for test_line in (TEST_CASES / (test_case["name"] + ".txt")).read_text().splitlines()]
    run_case(test_case["name"], test_tasks, test_depth=1 if test_index % 2 == 0 else 4)

if TEST_UNITS == 2 and not TEST_WINDOW:
    test_stage3_cases = Path(__file__).parents[1] / "stage3/cases"
    test_tasks = [tuple(map(int, test_line.split()))
                  for test_line in (test_stage3_cases / "capacity_pressure.txt").read_text().splitlines()]
    test_events, test_small = run_case("capacity_before", test_tasks, test_period=3, test_result_depth=1)
    test_events, test_fixed = run_case("capacity_after", test_tasks, test_period=3, test_result_depth=2)
    require(test_fixed["cycles"] < test_small["cycles"], "capacity improvement regressed")
    require(test_fixed["compute_result_wait_cycles"] < test_small["compute_result_wait_cycles"], "wait improvement")
    require(test_events["compute_complete"][1]["cycle"] < test_events["compute_complete"][0]["cycle"],
            "scenario did not produce out-of-order completion")
    require(test_fixed["reorder_wait_cycles"] > 0, "ordering pressure was not exercised")
    test_tasks = [tuple(map(int, test_line.split()))
                  for test_line in (test_stage3_cases / "round_robin_skew.txt").read_text().splitlines()]
    test_events, test_skew = run_case("round_robin_skew", test_tasks)
    require(test_skew["dispatch_idle_other_blocked_cycles"] > 0, "RR limitation not exercised")
    require(test_skew["compute0_busy_cycles"] > 10 * test_skew["compute1_busy_cycles"], "no load imbalance")

if TEST_WINDOW:
    test_events, test_metrics = run_case("zero_burst", [(test_i, 0) for test_i in range(40)],
                                         test_depth=4, test_result_depth=1)
    require(test_metrics["compute_result_wait_cycles"] > 0, "result pending state not exercised")
    test_tasks = [tuple(map(int, test_line.split())) for test_line in
                  (Path(__file__).parents[1] / "stage3/cases/head_long.txt").read_text().splitlines()]
    for test_window in (1, 2, 8):
        for test_seed in (1, 7, 42):
            test_events, test_metrics = run_case(f"head_w{test_window}_s{test_seed}", test_tasks,
                                                 test_window=test_window, test_seed=test_seed)
            require(test_metrics["window_reserved_peak"] == test_window, "window full not exercised")
            require(test_metrics["window_blocked_cycles"] > 0, "window backpressure not exercised")
            if test_window > 1:
                require(test_events["compute_complete"][1]["cycle"] < test_events["compute_complete"][0]["cycle"],
                        "out-of-order completion not exercised")
    run_case("window_slow_sink", test_tasks, test_depth=1, test_result_depth=1, test_period=13, test_window=2)
    run_case("head_repeat", test_tasks, test_window=8, test_seed=1)
    require((TEST_ROOT / "head_repeat/events.csv").read_bytes() == (TEST_ROOT / "head_w8_s1/events.csv").read_bytes(),
            "same-seed run is not reproducible")

for test_index, test_text in enumerate(["1\n", "a 2\n", "1 2 3\n", "2147483648 1\n", "\n", "1 2.5\n",
                                       "-2147483649 1\n", "1 2147483648\n", "1 -2147483649\n"]):
    test_folder = TEST_ROOT / f"invalid_{test_index}"
    test_folder.mkdir(exist_ok=True)
    (test_folder / "input.txt").write_text(test_text, encoding="utf-8")
    test_result = subprocess.run([TEST_EXE, *(str(test_folder / test_file)
                                             for test_file in ("input.txt", "output.txt", "stats.csv"))],
                                 capture_output=True, text=True, timeout=10)
    require(test_result.returncode != 0 and "invalid input line" in test_result.stderr, "invalid input accepted")
with (TEST_ROOT / "summary.csv").open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_summary[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_summary)
print(f"PASS {len(test_summary)} handshake scenarios and invalid inputs")
