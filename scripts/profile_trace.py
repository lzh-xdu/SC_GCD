"""Linux perf sampling and interleaved, unprofiled Trace regression measurements."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import shutil
import statistics
import subprocess
import sys
import tempfile
import time


def digest(test_path):
    test_hash = hashlib.sha256()
    with test_path.open("rb") as test_stream:
        for test_chunk in iter(lambda: test_stream.read(1024 * 1024), b""):
            test_hash.update(test_chunk)
    return test_hash.hexdigest()


def command(test_exe, test_folder, test_period=1, test_trace=True):
    return [str(test_exe), str(test_folder / "input.txt"), str(test_folder / "output.txt"),
            str(test_folder / "stats.csv"), "2",
            str(test_folder / "events.csv") if test_trace else "-",
            str(test_period), "1000000000", "2", "8", "7"]


def measure(test_command, test_folder, test_expected, test_trace):
    with (test_folder / "run.log").open("wb") as test_log:
        test_start = time.perf_counter()
        test_process = subprocess.Popen(test_command, stdout=test_log, stderr=test_log)
        _, test_status, test_usage = os.wait4(test_process.pid, 0)
        test_elapsed = time.perf_counter() - test_start
        test_process.returncode = os.waitstatus_to_exitcode(test_status)
    if test_process.returncode:
        raise RuntimeError((test_folder / "run.log").read_text())
    if (test_folder / "output.txt").read_text() != test_expected:
        raise AssertionError("independent GCD/output order mismatch")
    return {"wall_ms": test_elapsed * 1000, "user_ms": test_usage.ru_utime * 1000,
            "system_ms": test_usage.ru_stime * 1000, "peak_rss_kib": test_usage.ru_maxrss,
            "output_sha256": digest(test_folder / "output.txt"),
            "stats_sha256": digest(test_folder / "stats.csv"),
            "trace_sha256": digest(test_folder / "events.csv") if test_trace else None,
            "trace_bytes": (test_folder / "events.csv").stat().st_size if test_trace else 0}


def record(test_options, test_folder, test_pairs):
    (test_folder / "input.txt").write_text("".join(f"{test_a} {test_b}\n" for test_a, test_b in test_pairs))
    test_data = test_folder / "perf.data"
    test_cmd = [str(test_options.perf), "record", "-e", "cpu-clock:u", "-F", "997", "--call-graph", "fp",
                "-o", str(test_data), "--", sys.executable, str(Path(__file__).resolve()),
                "--worker", str(test_options.before), str(test_folder), str(test_options.repeat)]
    with (test_options.report / "record.log").open("w") as test_log:
        subprocess.run(test_cmd, stdout=test_log, stderr=subprocess.STDOUT, check=True)
    for test_label, test_children in (("self", "--no-children"), ("inclusive", "--children")):
        with (test_options.report / f"{test_label}.txt").open("w") as test_log:
            subprocess.run([str(test_options.perf), "report", "--stdio", test_children,
                            "--call-graph", "none", "--percent-limit", "0.5", "-i", str(test_data)],
                           stdout=test_log, stderr=subprocess.STDOUT, check=True)
    # Raw samples are a local reproduction artifact, never part of the published evidence.
    test_raw = Path(__file__).resolve().parent.parent / "tmp" / "perf-trace" / f"{test_options.report.name}.data"
    test_raw.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(test_data, test_raw)


def compare(test_options, test_folder, test_pairs):
    test_models = {"before": test_options.before, "after": test_options.after}
    if test_options.clock:
        test_models["clock"] = test_options.clock
    test_cases = [("mixed_on", test_pairs, 1, True), ("mixed_off", test_pairs, 1, False),
                  ("zero_on", [(7, 0)] * 10000, 1, True), ("slow_on", [(48, 18)] * 100, 1000, True)]
    test_rows = []
    test_mixed_statistics = None
    for test_name, test_input, test_period, test_trace in test_cases:
        (test_folder / "input.txt").write_text("".join(f"{test_a} {test_b}\n" for test_a, test_b in test_input))
        test_expected = "".join(f"{math.gcd(test_a, test_b)}\n" for test_a, test_b in test_input)
        test_hashes = None
        for test_round in range(-1, test_options.repeat):
            test_items = list(test_models.items())
            if test_round % 2:
                test_items.reverse()
            for test_label, test_exe in test_items:
                test_row = measure(command(test_exe, test_folder, test_period, test_trace),
                                   test_folder, test_expected, test_trace)
                test_current = [test_row[test_key] for test_key in
                                ("output_sha256", "stats_sha256", "trace_sha256")]
                if test_hashes is not None and test_current != test_hashes:
                    raise AssertionError(f"{test_name}/{test_label}: before/after/clock content mismatch")
                test_hashes = test_current
                if test_name.startswith("mixed_"):
                    if test_mixed_statistics is not None and test_current[:2] != test_mixed_statistics:
                        raise AssertionError("Trace on/off changed functional output or statistics")
                    test_mixed_statistics = test_current[:2]
                if test_round >= 0:
                    test_rows.append(dict(case=test_name, model=test_label, round=test_round, **test_row))
    (test_options.report / "samples.json").write_text(json.dumps(test_rows, indent=2) + "\n")
    test_summary = []
    for test_name, _, _, _ in test_cases:
        for test_label in test_models:
            test_selected = [test_row for test_row in test_rows
                             if test_row["case"] == test_name and test_row["model"] == test_label]
            test_summary.append(dict(case=test_name, model=test_label, **{
                test_key: statistics.median(test_row[test_key] for test_row in test_selected)
                for test_key in ("wall_ms", "user_ms", "system_ms", "peak_rss_kib", "trace_bytes")}))
    (test_options.report / "summary.json").write_text(json.dumps(test_summary, indent=2) + "\n")
    print(json.dumps(test_summary, indent=2))


def main():
    if sys.argv[1:2] == ["--worker"]:
        test_exe, test_folder, test_repeat = sys.argv[2:]
        for _ in range(int(test_repeat)):
            subprocess.run(command(Path(test_exe), Path(test_folder)), check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return
    test_parser = argparse.ArgumentParser(description=__doc__)
    test_parser.add_argument("--before", type=Path, required=True)
    test_parser.add_argument("--after", type=Path)
    test_parser.add_argument("--clock", type=Path)
    test_parser.add_argument("--perf", type=Path)
    test_parser.add_argument("--report", type=Path, required=True)
    test_parser.add_argument("--repeat", type=int, default=7)
    test_options = test_parser.parse_args()
    if test_options.repeat < 1 or bool(test_options.perf) == bool(test_options.after):
        test_parser.error("choose perf recording or before/after comparison; repeat must be positive")
    for test_key in ("before", "after", "clock", "perf", "report"):
        if getattr(test_options, test_key):
            setattr(test_options, test_key, getattr(test_options, test_key).resolve())
    test_options.report.mkdir(parents=True, exist_ok=True)
    test_metadata = {"platform": platform.platform(), "python": sys.version,
                     "seed": 20260910, "mixed_tasks": 10000, "repeat": test_options.repeat,
                     "argv": sys.argv, "scope": "whole process; Linux native temporary output files",
                     "baseline_revision": subprocess.check_output(
                         ["git", "rev-parse", "HEAD"], text=True).strip(),
                     "binaries": {test_key: {"path": str(getattr(test_options, test_key)),
                                              "sha256": digest(getattr(test_options, test_key))}
                                  for test_key in ("before", "after", "clock", "perf")
                                  if getattr(test_options, test_key)}}
    (test_options.report / "metadata.json").write_text(json.dumps(test_metadata, indent=2) + "\n")
    test_random = random.Random(20260910)
    test_pairs = [(test_random.randint(-2147483648, 2147483647),
                   test_random.randint(-2147483648, 2147483647)) for _ in range(10000)]
    with tempfile.TemporaryDirectory(prefix="moore-perf-trace-") as test_directory:
        if test_options.perf:
            record(test_options, Path(test_directory), test_pairs)
        else:
            compare(test_options, Path(test_directory), test_pairs)


if __name__ == "__main__":
    main()
