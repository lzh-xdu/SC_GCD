"""Compare simulated performance and declared resources; no silicon/PPA claims."""
import csv
import math
from pathlib import Path
import subprocess
import sys

TEST_SINGLE = str(Path(sys.argv[1]).resolve())
TEST_DUAL = str(Path(sys.argv[2]).resolve())
TEST_ROOT = Path(sys.argv[3]).resolve()
TEST_SOURCE = Path(__file__).parents[1]
TEST_ROOT.mkdir(parents=True, exist_ok=True)


def run_model(test_exe, test_name, test_source, test_period, test_units):
    test_folder = TEST_ROOT / f"{test_name}_units{test_units}"
    test_folder.mkdir(exist_ok=True)
    test_command = [test_exe, str(test_source), str(test_folder / "output.txt"), str(test_folder / "stats.csv"),
                    "2", "-", str(test_period), "1000000"]
    if test_units == 2:
        test_command.append("2")
    test_result = subprocess.run(test_command, capture_output=True, text=True, timeout=15)
    (test_folder / "run.log").write_text(test_result.stdout + test_result.stderr, encoding="utf-8")
    if test_result.returncode:
        raise AssertionError(test_result.stderr)
    test_tasks = [tuple(map(int, test_line.split())) for test_line in test_source.read_text().splitlines()]
    test_output = (test_folder / "output.txt").read_text()
    if test_output != "".join(f"{math.gcd(*test_task)}\n" for test_task in test_tasks):
        raise AssertionError("wrong comparison output")
    return {test_row["metric"]: float(test_row["value"])
            for test_row in csv.DictReader((test_folder / "stats.csv").open())}, test_output


test_rows = []
for test_name, test_file, test_period in [
    ("basic", "stage1/basic.txt", 1),
    ("fibonacci", "stage1/cases/fibonacci_consecutive.txt", 1),
    ("random", "stage1/cases/random_01.txt", 1),
    ("mixed", "stage1/cases/short_with_long.txt", 1),
    ("capacity_pressure", "stage3/cases/capacity_pressure.txt", 3),
    ("round_robin_skew", "stage3/cases/round_robin_skew.txt", 1),
]:
    test_single, test_output1 = run_model(TEST_SINGLE, test_name, TEST_SOURCE / test_file, test_period, 1)
    test_dual, test_output2 = run_model(TEST_DUAL, test_name, TEST_SOURCE / test_file, test_period, 2)
    assert test_output1 == test_output2
    assert test_single["compute_busy_cycles"] == test_dual["compute_busy_cycles"]
    test_speedup = test_single["cycles"] / test_dual["cycles"]
    test_rows.append({"case": test_name, "tasks": int(test_single["tasks"]), "output_period": test_period,
                      "single_cycles": int(test_single["cycles"]), "dual_cycles": int(test_dual["cycles"]),
                      "speedup": test_speedup, "speedup_per_compute": test_speedup / 2,
                      "single_utilization": test_single["compute_utilization"],
                      "dual_utilization": test_dual["compute_utilization"],
                      "single_fifo_slots": test_single["fifo_slots"], "dual_fifo_slots": test_dual["fifo_slots"],
                      "single_buffer_payload_bits": test_single["buffer_payload_bits"],
                      "dual_buffer_payload_bits": test_dual["buffer_payload_bits"]})
    print(f"PASS {test_name}: {int(test_single['cycles'])} -> {int(test_dual['cycles'])} cycles; {test_speedup:.4f}x")
with (TEST_ROOT / "comparison.csv").open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_rows[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_rows)
