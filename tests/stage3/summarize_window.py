"""Join baseline/window measurements, verifying input and arithmetic work equality."""
import csv
from pathlib import Path
import statistics
import sys

TEST_BASELINE = Path(sys.argv[1])
TEST_NEW = Path(sys.argv[2])
TEST_OUTPUT = Path(sys.argv[3])
test_old = list(csv.DictReader(TEST_BASELINE.open()))
test_new = list(csv.DictReader(TEST_NEW.open()))
test_rows = []
for test_case in dict.fromkeys(test_row["case"] for test_row in test_old):
    test_base = next(test_row for test_row in test_old if test_row["case"] == test_case and test_row["result_depth"] == "2")
    test_matched = next(test_row for test_row in test_old if test_row["case"] == test_case and test_row["result_depth"] == "6")
    test_samples = [test_row for test_row in test_new if test_row["case"] == test_case and test_row["window"] == "8"]
    for test_row in [test_matched, *test_samples]:
        assert all(test_row[test_key] == test_base[test_key]
                   for test_key in ("input_sha256", "output_period", "busy_cycles"))
    assert all(test_row["payload_bits"] == test_matched["payload_bits"] for test_row in test_samples)
    test_mean = statistics.mean(int(test_row["cycles"]) for test_row in test_samples)
    test_rows.append({"case": test_case, "old_cycles": test_base["cycles"], "old_payload_matched_cycles": test_matched["cycles"],
                      "new_cycles_min": min(int(test_row["cycles"]) for test_row in test_samples),
                      "new_cycles_max": max(int(test_row["cycles"]) for test_row in test_samples),
                      "new_cycles_mean": test_mean, "speedup": int(test_base["cycles"]) / test_mean,
                      "old_payload_bits": test_base["payload_bits"], "new_payload_bits": test_samples[0]["payload_bits"]})
with TEST_OUTPUT.open("w", newline="", encoding="utf-8") as test_stream:
    test_writer = csv.DictWriter(test_stream, fieldnames=test_rows[0].keys())
    test_writer.writeheader()
    test_writer.writerows(test_rows)
print(f"PASS {len(test_rows)} input/work-matched comparisons: {TEST_OUTPUT}")
