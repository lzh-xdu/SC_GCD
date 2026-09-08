"""Measure host process time separately from simulated cycles and model statistics."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    command = options.command[1:] if options.command[:1] == ["--"] else options.command
    if options.repeat < 1 or len(command) < 4:
        parser.error("repeat must be positive; command must include executable, input, output and stats")
    # Avoid replacing a model file with the host profiling report.
    report = options.report.resolve()
    if any(report == Path(argument).resolve() for argument in command):
        parser.error("report must be distinct from every model command path")
    baseline = None
    samples = []
    for index in range(options.repeat):
        start = time.perf_counter()
        result = subprocess.run(command, capture_output=True, timeout=120, check=False)
        seconds = time.perf_counter() - start
        if result.returncode:
            raise RuntimeError(f"run {index}: {result.stderr.decode(errors='replace')}")
        hashes = [hashlib.sha256(Path(command[position]).read_bytes()).hexdigest() for position in (2, 3)]
        if baseline is not None and hashes != baseline:
            raise RuntimeError("repeated runs changed functional output or simulated statistics")
        baseline = hashes
        samples.append(seconds)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({
        "command": command,
        "scope": "whole host process including startup, simulation, instrumentation and file I/O",
        "samples_seconds": samples,
        "median_seconds": statistics.median(samples),
        "output_sha256": baseline[0],
        "statistics_sha256": baseline[1],
    }, indent=2) + "\n", encoding="utf-8")
    print(f"PASS {options.repeat} identical output/statistics runs; profile: {report}")


if __name__ == "__main__":
    main()
