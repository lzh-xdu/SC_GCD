"""Compare the existing trace switch using identical model configuration."""
from pathlib import Path
import subprocess


def check_trace_equivalence(test_command, test_folder):
    test_off_command = test_command.copy()
    test_off_command[2] = str(test_folder / "untraced-output.txt")
    test_off_command[3] = str(test_folder / "untraced-stats.csv")
    test_off_command[5] = "-"
    test_run = subprocess.run(test_off_command, capture_output=True, text=True, timeout=15)
    (test_folder / "untraced-run.log").write_text(test_run.stdout + test_run.stderr, encoding="utf-8")
    if test_run.returncode:
        raise AssertionError(f"{test_folder.name}: trace-off failed: {test_run.stderr}")
    for test_index in (2, 3):
        if Path(test_command[test_index]).read_bytes() != Path(test_off_command[test_index]).read_bytes():
            raise AssertionError(f"{test_folder.name}: trace switch changed {test_command[test_index]}")
