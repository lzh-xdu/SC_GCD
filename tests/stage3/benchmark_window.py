"""Capture old policy first, then compare identical inputs against a bounded window."""
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

TEST_MODE = sys.argv[1]
TEST_EXE = str(Path(sys.argv[2]).resolve())
TEST_ROOT = Path(sys.argv[3]).resolve()
TEST_SOURCE = Path(__file__).parents[1]
TEST_ROOT.mkdir(parents=True, exist_ok=True)
TEST_SEEDS = (1, 7, 42, 20260906)


def cases():
    test_cases = [("basic", "stage1/basic.txt", 1),
                  ("fibonacci", "stage1/cases/fibonacci_consecutive.txt", 1),
                  ("mixed", "stage1/cases/short_with_long.txt", 1),
                  ("pressure", "stage3/cases/capacity_pressure.txt", 3),
                  ("skew", "stage3/cases/round_robin_skew.txt", 1),
                  ("head_long", "stage3/cases/head_long.txt", 1)]
    test_cases += [(f"random_{test_i:02d}", f"stage1/cases/random_{test_i:02d}.txt", 1)
                   for test_i in range(1, 11)]
    return test_cases


def run_case(test_name, test_file, test_period, test_result_depth, test_window=0, test_seed=0):
    test_label = f"{test_name}_r{test_result_depth}_w{test_window}_s{test_seed}"
    test_folder = TEST_ROOT / test_label
    test_folder.mkdir(exist_ok=True)
    test_source = TEST_SOURCE / test_file
    test_command = [TEST_EXE, str(test_source), str(test_folder / "output.txt"), str(test_folder / "stats.csv"),
                    "2", "-", str(test_period), "1000000", str(test_result_depth)]
    if test_window:
        test_command += [str(test_window), str(test_seed)]
    test_result = subprocess.run(test_command, capture_output=True, text=True, timeout=15)
    (test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    if test_result.returncode:
        raise AssertionError(test_result.stderr)
    test_tasks = [tuple(map(int, test_line.split())) for test_line in test_source.read_text().splitlines()]
    if (test_folder / "output.txt").read_text() != "".join(f"{math.gcd(*test_task)}\n" for test_task in test_tasks):
        raise AssertionError(test_label + ": functional mismatch")
    test_metrics = {test_row["metric"]: float(test_row["value"])
                    for test_row in csv.DictReader((test_folder / "stats.csv").open())}
    return {"case": test_name, "result_depth": test_result_depth, "window": test_window, "seed": test_seed,
            "input_sha256": hashlib.sha256(test_source.read_bytes()).hexdigest(),
            "output_period": test_period, "cycles": int(test_metrics["cycles"]),
            "busy_cycles": int(test_metrics["compute_busy_cycles"]),
            "utilization": test_metrics["compute_utilization"],
            "result_wait": int(test_metrics["compute_result_wait_cycles"]),
            "payload_bits": int(test_metrics["buffer_payload_bits"])}


test_rows = []
for test_name, test_file, test_period in cases():
    if TEST_MODE == "baseline":
        for test_depth in (2, 6):
            test_rows.append(run_case(test_name, test_file, test_period, test_depth))
    elif TEST_MODE == "window":
        for test_seed in TEST_SEEDS:
            test_rows.append(run_case(test_name, test_file, test_period, 2, 8, test_seed))
    else:
        raise ValueError("expected baseline or window")
if TEST_MODE == "window":
    for test_window in (1, 2, 4, 16):
        test_rows.append(run_case("head_long", "stage3/cases/head_long.txt", 1, 2, test_window, 1))
with (TEST_ROOT / "summary.csv").open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_rows[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_rows)
(TEST_ROOT / "manifest.json").write_text(json.dumps({"mode": TEST_MODE, "executable": TEST_EXE,
    "executable_sha256": hashlib.sha256(Path(TEST_EXE).read_bytes()).hexdigest(),
    "cases": len(test_rows), "depth": 2, "trace": False}, indent=2) + "\n", encoding="utf-8")
print(f"PASS {TEST_MODE}: {len(test_rows)} verified runs; {TEST_ROOT / 'summary.csv'}")
