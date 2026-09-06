"""Targeted configuration sweeps for review; leaves the architecture unchanged."""
import csv
import math
from pathlib import Path
import subprocess
import sys

TEST_BUILD = Path(sys.argv[1]).resolve()
TEST_ROOT = Path(sys.argv[2]).resolve()
TEST_SOURCE = Path(__file__).parents[1]
TEST_ROOT.mkdir(parents=True, exist_ok=True)
test_rows = []


def run_case(test_model, test_name, test_tasks, test_depth=2, test_result_depth=2, test_window=8, test_period=1):
    test_label = f"{test_model}_{test_name}_d{test_depth}_r{test_result_depth}_w{test_window}_p{test_period}"
    test_folder = TEST_ROOT / test_label
    test_folder.mkdir(exist_ok=True)
    (test_folder / "input.txt").write_text("".join(f"{test_a} {test_b}\n" for test_a, test_b in test_tasks), encoding="utf-8")
    test_command = [str(TEST_BUILD / f"{test_model}_gcd.exe"),
                    *(str(test_folder / test_name) for test_name in ("input.txt", "output.txt", "stats.csv")),
                    str(test_depth), "-", str(test_period), "1000000"]
    if test_model.startswith("stage3"):
        test_command.append(str(test_result_depth))
    if test_model == "stage3_window":
        test_command.extend((str(test_window), "1"))
    test_result = subprocess.run(test_command, capture_output=True, text=True, timeout=15)
    if test_result.returncode:
        raise AssertionError(test_result.stderr)
    if (test_folder / "output.txt").read_text() != "".join(f"{math.gcd(*test_task)}\n" for test_task in test_tasks):
        raise AssertionError("functional mismatch: " + test_label)
    test_metrics = {test_row["metric"]: float(test_row["value"])
                    for test_row in csv.DictReader((test_folder / "stats.csv").open())}
    test_rows.append({"model": test_model, "case": test_name, "depth": test_depth,
                      "result_depth": test_result_depth if test_model.startswith("stage3") else "",
                      "window": test_window if test_model == "stage3_window" else 0, "period": test_period,
                      "cycles": int(test_metrics["cycles"]), "busy": int(test_metrics["compute_busy_cycles"]),
                      "result_wait": int(test_metrics["compute_result_wait_cycles"]),
                      "window_blocked": int(test_metrics["window_blocked_cycles"]) if "window_blocked_cycles" in test_metrics else "",
                      "payload_bits": int(test_metrics["buffer_payload_bits"]) if "buffer_payload_bits" in test_metrics else ""})


for test_model in ("stage1", "stage2"):
    for test_depth in (1, 2, 4):
        run_case(test_model, "zero_stream", [(test_i, 0) for test_i in range(1, 41)], test_depth)

for test_result_depth in (1, 2, 4):
    run_case("stage3_window", "zero_burst", [(test_i, 0) for test_i in range(40)],
             test_depth=4, test_result_depth=test_result_depth)

for test_name, test_file, test_period in [
    ("basic", "stage1/basic.txt", 1), ("fib", "stage1/cases/fibonacci_consecutive.txt", 1),
    ("random", "stage1/cases/random_01.txt", 1), ("mixed", "stage1/cases/short_with_long.txt", 1),
    ("skew", "stage3/cases/round_robin_skew.txt", 1), ("head", "stage3/cases/head_long.txt", 1),
    ("pressure", "stage3/cases/capacity_pressure.txt", 3),
]:
    test_tasks = [tuple(map(int, test_line.split())) for test_line in (TEST_SOURCE / test_file).read_text().splitlines()]
    for test_window in (2, 4, 8, 16, 32):
        run_case("stage3_window", test_name, test_tasks, test_window=test_window, test_period=test_period)
    for test_result_depth in (1, 4):
        run_case("stage3_window", test_name, test_tasks, test_result_depth=test_result_depth, test_period=test_period)

with (TEST_ROOT / "summary.csv").open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_rows[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_rows)
print(f"PASS {len(test_rows)} configuration runs; all outputs independently checked")
