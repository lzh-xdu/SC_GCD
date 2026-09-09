"""Reproducible serial, interleaved Windows comparison of stage 3 and stage 4."""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import re
import statistics
import subprocess
import time
import winreg


def processor_name():
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
        return winreg.QueryValueEx(key, "ProcessorNameString")[0]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cases():
    random_source = random.Random(20260910)
    mixed = [(random_source.randint(-2147483648, 2147483647),
              random_source.randint(-2147483648, 2147483647)) for _ in range(10000)]
    return [
        ("dense_zero", [(7, 0)] * 30000, 1, 8, False),
        ("long_compute", [(1836311903, 1134903170)] * 10000, 1, 8, False),
        ("mixed", mixed, 1, 8, False),
        ("slow_sink", [(48, 18)] * 2000, 1000, 8, False),
        ("sparse_wait", [(48, 18)], 2000000, 8, False),
        ("narrow_window", ([(1836311903, 1134903170)] + [(7, 0)] * 20) * 500, 1, 1, False),
        ("dense_small_off", [(7, 0)] * 2000, 1, 8, False),
        ("mixed_small_off", mixed[:1000], 1, 8, False),
        ("dense_trace", [(7, 0)] * 2000, 1, 8, True),
        ("mixed_trace", mixed[:1000], 1, 8, True),
    ]


def run_model(exe, folder, input_file, period, window, trace):
    folder.mkdir(parents=True, exist_ok=True)
    paths = [folder / name for name in ("output.txt", "stats.csv", "events.csv")]
    command = [str(exe), str(input_file), str(paths[0]), str(paths[1]), "2",
               str(paths[2]) if trace else "-", str(period), "100000000", "2", str(window), "7"]
    started = time.perf_counter()
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    elapsed_ms = (time.perf_counter() - started) * 1000
    (folder / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr)
    fields = {key: float(value) for key, value in re.findall(r"(\w+)=([\d.]+)", result.stderr)}
    fields.update(process_ms=elapsed_ms, output_sha256=digest(paths[0]), stats_sha256=digest(paths[1]),
                  trace_sha256=digest(paths[2]) if trace else None,
                  trace_bytes=paths[2].stat().st_size if trace else 0)
    return fields, paths


def summarize(samples, name, model):
    rows = [row for row in samples if row["case"] == name and row["model"] == model]
    summary = {"case": name, "model": model, "samples": len(rows)}
    for key in ("model_ms", "process_ms", "cpu_ms", "peak_working_set_bytes", "peak_commit_bytes", "delta_cycles"):
        values = [row[key] for row in rows]
        summary[key] = statistics.median(values)
        if key.endswith("ms"):
            quartiles = statistics.quantiles(values, n=4, method="inclusive")
            summary[key + "_q1"] = quartiles[0]
            summary[key + "_q3"] = quartiles[2]
            summary[key + "_min"] = min(values)
            summary[key + "_max"] = max(values)
    summary["activations"] = rows[0].get("activations")
    summary["max_jump_cycles"] = rows[0].get("max_jump_cycles")
    summary["trace_bytes"] = rows[0]["trace_bytes"]
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--work", type=Path, default=Path("tmp/model-comparison"))
    parser.add_argument("--report", type=Path, default=Path("docs/evidence/model-comparison"))
    parser.add_argument("--repeat", type=int, default=7)
    parser.add_argument("--cases", nargs="+", help="optional subset of named cases")
    options = parser.parse_args()
    if options.repeat < 4:
        parser.error("at least four samples are required for quartiles")
    build, work, report = (path.resolve() for path in (options.build, options.work, options.report))
    report.mkdir(parents=True, exist_ok=True)
    models = {"clock": (build / "stage3_window_gcd.exe", build / "stage3_window_profile.exe"),
              "event": (build / "stage4_gcd.exe", build / "stage4_profile.exe")}
    metadata = {"baseline": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
                "processor_name": processor_name(),
                "platform": platform.platform(), "processor": platform.processor(),
                "logical_processors": os.cpu_count(), "python": platform.python_version(),
                "compiler": subprocess.check_output(["g++", "--version"], text=True).splitlines()[0],
                "systemc_revision": subprocess.check_output(["git", "-C", "third_party/systemc", "rev-parse", "HEAD"],
                                                            text=True).strip(),
                "repeat": options.repeat, "warmups_per_case_model": 1,
                "order": "serial; alternating clock/event and event/clock per repetition",
                "measurement_scope": "Release; model call includes construction, simulation, statistics and file IO; process includes startup",
                "executables": {key: {"normal": digest(value[0]), "profile": digest(value[1])}
                                for key, value in models.items()}}
    (report / "environment.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    samples, summaries, manifest = [], [], []
    with (report / "samples.jsonl").open("w", encoding="utf-8") as sample_stream:
        for name, tasks, period, window, trace in cases():
            if options.cases and name not in options.cases:
                continue
            input_file = work / name / "input.txt"
            input_file.parent.mkdir(parents=True, exist_ok=True)
            input_file.write_text("".join(f"{a} {b}\n" for a, b in tasks), encoding="utf-8")
            expected = [math.gcd(a, b) for a, b in tasks]
            reference = None
            for model, (normal, profile) in models.items():
                for phase, exe in (("normal", normal), ("warmup", profile)):
                    observation, paths = run_model(exe, work / name / model, input_file, period, window, trace)
                    if [int(line) for line in paths[0].read_text().splitlines()] != expected:
                        raise AssertionError(f"independent GCD oracle mismatch: {name}/{model}/{phase}")
                    hashes = [observation[key] for key in ("output_sha256", "stats_sha256", "trace_sha256")]
                    if reference is not None and hashes != reference:
                        raise AssertionError(f"model/wrapper equivalence failed: {name}/{model}/{phase}")
                    reference = hashes
            with paths[1].open() as stream:
                metrics = {key: float(value) for key, value in list(csv.reader(stream))[1:]}
            manifest.append({"case": name, "tasks": len(tasks), "period": period, "window": window,
                             "depth": 2, "result_depth": 2, "seed": 7, "trace": trace,
                             "input_sha256": digest(input_file), "reference_hashes": reference, "metrics": metrics})
            for repetition in range(options.repeat):
                order = ("clock", "event") if repetition % 2 == 0 else ("event", "clock")
                for model in order:
                    observation, _ = run_model(models[model][1], work / name / model, input_file, period, window, trace)
                    hashes = [observation[key] for key in ("output_sha256", "stats_sha256", "trace_sha256")]
                    if hashes != reference:
                        raise AssertionError(f"repeat changed output/statistics/trace: {name}/{model}/{repetition}")
                    observation.update(case=name, model=model, repetition=repetition)
                    samples.append(observation)
                    sample_stream.write(json.dumps(observation) + "\n")
                    sample_stream.flush()
            pair = [summarize(samples, name, model) for model in models]
            summaries.extend(pair)
            print(f"PASS {name}: cycles={int(metrics['cycles'])}; model median ms clock={pair[0]['model_ms']:.3f}, "
                  f"event={pair[1]['model_ms']:.3f}, speedup={pair[0]['model_ms']/pair[1]['model_ms']:.2f}x", flush=True)
    for filename, content in (("cases.json", manifest), ("summary.json", summaries)):
        (report / filename).write_text(json.dumps(content, indent=2) + "\n", encoding="utf-8")
    with (report / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=summaries[0].keys())
        writer.writeheader()
        writer.writerows(summaries)


if __name__ == "__main__":
    main()
