"""Run independent protocol checks on both models, then compare their artifacts."""
from pathlib import Path
import subprocess
import sys


def compare_events(test_baseline, test_candidate):
    # Parser now runs in its own timed thread. Only its position relative to other
    # processes within the same timestamp may differ; preserve every stream's order.
    test_old_rows = test_baseline.read_text().splitlines()
    test_new_rows = test_candidate.read_text().splitlines()
    for test_rows in (test_old_rows, test_new_rows):
        test_cycles = [int(test_row.split(",")[2]) for test_row in test_rows[1:]]
        if test_cycles != sorted(test_cycles):
            raise AssertionError("event rows moved across timestamps")
    for test_parser_stream in (False, True):
        test_old_stream = [test_row for test_row in test_old_rows
                           if (",parser_send," in test_row) == test_parser_stream]
        test_new_stream = [test_row for test_row in test_new_rows
                           if (",parser_send," in test_row) == test_parser_stream]
        if test_old_stream != test_new_stream:
            raise AssertionError(f"event content, timestamp or stream order differs: {test_candidate}")


def compare_initial(test_baseline, test_candidate, test_root):
    test_verifier = Path(__file__).resolve().parents[1] / "handshake/verify.py"
    for test_name, test_executable in (("baseline", test_baseline), ("candidate", test_candidate)):
        subprocess.run([sys.executable, str(test_verifier), str(Path(test_executable).resolve()),
                        str(test_root / test_name), "2", "8"], check=True, timeout=50)
    test_baseline_root = test_root / "baseline"
    test_candidate_root = test_root / "candidate"
    test_files = {test_path.relative_to(test_baseline_root)
                  for test_path in test_baseline_root.rglob("*") if test_path.is_file()}
    test_candidate_files = {test_path.relative_to(test_candidate_root)
                            for test_path in test_candidate_root.rglob("*") if test_path.is_file()}
    if test_files != test_candidate_files:
        raise AssertionError("baseline/candidate artifact sets differ")
    for test_relative in sorted(test_files):
        if test_relative.name == "events.csv":
            compare_events(test_baseline_root / test_relative, test_candidate_root / test_relative)
            continue
        if (test_baseline_root / test_relative).read_bytes() != (test_candidate_root / test_relative).read_bytes():
            raise AssertionError(f"artifact differs: {test_relative}")
    print(f"PASS equivalence: {len(test_files)} artifacts; event streams match, other files identical")


if __name__ == "__main__":
    compare_initial(sys.argv[1], sys.argv[2], Path(sys.argv[3]).resolve())
