"""Compare service boundaries and prove that long inert intervals are skipped."""
import csv
from pathlib import Path
import re
import subprocess
import sys


def run_case(test_baseline, test_candidate, test_root, test_name, test_input, test_period, test_window):
    test_folder = test_root / test_name
    test_folder.mkdir(parents=True, exist_ok=True)
    test_input_file = test_folder / "input.txt"
    test_input_file.write_text(test_input, encoding="utf-8")
    test_runs = []
    for test_label, test_exe in (("stage3", test_baseline), ("stage4", test_candidate)):
        test_output = test_folder / f"{test_label}.output.txt"
        test_stats = test_folder / f"{test_label}.stats.csv"
        test_run = subprocess.run([test_exe, str(test_input_file), str(test_output), str(test_stats),
                                   "1", "-", str(test_period), "2000000", "1", str(test_window), "7"],
                                  capture_output=True, text=True, timeout=45)
        (test_folder / f"{test_label}.log").write_text(test_run.stdout + test_run.stderr, encoding="utf-8")
        if test_run.returncode != 0:
            raise AssertionError(test_run.stderr)
        test_runs.append((test_output.read_bytes(), test_stats.read_bytes(), test_run.stderr))
    if test_runs[0][:2] != test_runs[1][:2]:
        raise AssertionError(f"result or timing/statistics differ: {test_name}")
    test_match = re.search(r"EVENT_SCHEDULER activations=(\d+) max_jump_cycles=(\d+)", test_runs[1][2])
    if not test_match:
        raise AssertionError("missing scheduler observation")
    test_metrics = dict(list(csv.reader(test_runs[1][1].decode().splitlines()))[1:])
    return {"case": test_name, "stage3_cycles": int(test_metrics["cycles"]),
            "stage4_cycles": int(test_metrics["cycles"]), "activations": int(test_match[1]),
            "max_jump_cycles": int(test_match[2])}


def main():
    test_baseline, test_candidate = (str(Path(test_arg).resolve()) for test_arg in sys.argv[1:3])
    test_root = Path(sys.argv[3]).resolve()
    test_cases = [
        ("long_sink", "48 18\n", 1000000, 1),
        ("zero_capacity_one", "0 0\n7 0\n0 -9\n" * 8, 13, 1),
        ("simultaneous_results", "48 18\n100 25\n" * 16, 1, 2),
        ("head_long", "1836311903 1134903170\n" + "1 0\n" * 40, 7, 8),
    ]
    test_rows = [run_case(test_baseline, test_candidate, test_root, *test_case) for test_case in test_cases]
    if test_rows[0]["stage4_cycles"] != 1000000 or test_rows[0]["activations"] > 12:
        raise AssertionError("long output wait is polled or its deadline changed")
    if test_rows[0]["max_jump_cycles"] < 999000:
        raise AssertionError("scheduler failed to skip the inert interval")
    with (test_root / "comparison.csv").open("w", newline="", encoding="utf-8") as test_stream:
        test_writer = csv.DictWriter(test_stream, fieldnames=test_rows[0].keys())
        test_writer.writeheader()
        test_writer.writerows(test_rows)
    for test_row in test_rows:
        print(f"PASS {test_row}")


if __name__ == "__main__":
    main()
