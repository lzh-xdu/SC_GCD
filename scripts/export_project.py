"""Export the submission package into a temporary folder at the project root.

Collects the six required categories into one self-describing tree:

    export/
    ├── README.txt            manifest: provenance, layout, reproduction
    ├── src/                  all SystemC sources (common/model/stage1-4)
    ├── build/                CMakeLists.txt, .clang-format, setup/build scripts
    ├── tests/                test scripts, case inputs and expected outputs
    │   └── metrics-inputs/   generated scenario inputs (deduplicated by content)
    ├── results/
    │   ├── functional/       one GCD output file per measured run
    │   └── performance/
    │       ├── runs/         one stats CSV per measured run
    │       ├── summary/      aggregated tables, plots and timelines (metrics matrix)
    │       └── evidence/     remaining historical evidence trees
    └── docs/                 design documents (top-level *.md)

The destination is disposable (recreated on every run) and must stay out of
version control; see /export/ in .gitignore. Nothing here modifies measured
data: files are copied verbatim.
"""
from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
from datetime import datetime
from pathlib import Path

IGNORE_PATTERNS = shutil.ignore_patterns("__pycache__", "*.pyc", "*.pyo")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy_tree(source: Path, target: Path, note: str, log: list[str]):
    if not source.is_dir():
        log.append(f"MISSING  {note}: {source} not found")
        return 0
    shutil.copytree(source, target, ignore=IGNORE_PATTERNS)
    count = sum(1 for item in target.rglob("*") if item.is_file())
    log.append(f"OK       {note}: {count} files <- {source}")
    return count


def copy_file(source: Path, target: Path, note: str, log: list[str]):
    if not source.is_file():
        log.append(f"MISSING  {note}: {source} not found")
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    log.append(f"OK       {note}: {source.name}")


def run_tag(folder: Path) -> str:
    """Strip the profile- prefix; profiling folders repeat identical outputs."""
    name = folder.name
    return name[len("profile-"):] if name.startswith("profile-") else name


def collect_runs(work: Path, results: Path, log: list[str]):
    functional = results / "functional"
    performance = results / "performance" / "runs"
    inputs = results.parent / "tests" / "metrics-inputs"
    functional.mkdir(parents=True, exist_ok=True)
    performance.mkdir(parents=True, exist_ok=True)
    inputs.mkdir(parents=True, exist_ok=True)
    seen_runs, seen_cases = set(), {}
    if not work.is_dir():
        log.append(f"MISSING  measured runs: {work} not found (rerun run_metrics_matrix.py)")
        return 0
    for folder in sorted(path for path in work.iterdir() if path.is_dir()):
        tag = run_tag(folder)
        output, stats, scenario_input = (folder / name for name in ("output.txt", "stats.csv", "input.txt"))
        if tag not in seen_runs and output.is_file() and stats.is_file():
            # Profiling repeats overwrite these files; content is verified
            # identical to the first repetition by run_metrics_matrix.py.
            shutil.copy2(output, functional / f"{tag}.output.txt")
            shutil.copy2(stats, performance / f"{tag}.stats.csv")
            seen_runs.add(tag)
        if scenario_input.is_file():
            case = tag.split("-")[1] if "-" in tag else tag
            checksum = digest(scenario_input)
            if case in seen_cases:
                if seen_cases[case] != checksum:
                    log.append(f"WARNING  scenario {case} has conflicting inputs; keeping the first copy")
                continue
            copy_file(scenario_input, inputs / f"{case}.txt", f"scenario input {case}", log)
            seen_cases[case] = checksum
    log.append(f"OK       measured runs: {len(seen_runs)} unique configurations, "
               f"{len(seen_cases)} scenario inputs")
    return len(seen_runs)


def write_manifest(export: Path, log: list[str], options):
    lines = [
        "SC_GCD 提交导出（Submission Export）",
        "=" * 72,
        f"生成时间：{datetime.now().isoformat(timespec='seconds')}",
        f"Git HEAD：{options.git_head}",
        "",
        "目录内容：",
        "  src/                     所有 SystemC 源代码（common/model/stage1~4）",
        "  build/                   编译配置：CMakeLists.txt、.clang-format、构建脚本",
        "  tests/                   测试脚本、用例输入与期望输出；",
        "      metrics-inputs/     性能矩阵场景输入（每场景一份）",
        "  results/functional/     每个测量配置的 GCD 功能输出（每行一个最终结果）",
        "  results/performance/    runs/ 每配置统计 CSV；summary/ 汇总表、图、时间线；",
        "                          evidence/ 其余历史测量证据",
        "  docs/                   设计文档（docs/*.md）",
        "",
        "复制日志：",
    ]
    lines += [f"  {entry}" for entry in log]
    counts = {}
    for category, folder in (("src", "src"), ("build", "build"), ("tests", "tests"),
                             ("results", "results"), ("docs", "docs")):
        tree = export / folder
        counts[category] = sum(1 for item in tree.rglob("*") if item.is_file()) if tree.is_dir() else 0
    total = sum(counts.values())
    lines += [
        "",
        "文件计数："
        + "，".join(f"{name}={count}" for name, count in counts.items())
        + f"，总计={total}",
        "",
        "复现步骤（Linux/WSL；Windows 用 scripts/setup.ps1 与 build-test.ps1）：",
        "  cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++",
        "  cmake --build build-linux -j12                # 或 --target stage1_gcd ... stage4_gcd",
        "  ctest --test-dir build-linux                 # 25/25 通过后测量",
        "  python3 scripts/run_metrics_matrix.py        # 重建 results/ 与 summary/ 数据",
        "  python3 scripts/export_project.py            # 重建本导出文件夹",
        "",
        "运行配置命名：<model>-<case>-D<depth>-P<period>[-R<result_depth>[-W<window>-S<seed>]]-trace|notrace。",
        "场景说明与全部结论见 docs/metrics-results.md；架构与设计决策见 docs/final-design.md。",
    ]
    (export / "README.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", type=Path, default=Path("export"), help="temporary export folder (recreated)")
    parser.add_argument("--work", type=Path, default=Path("tmp/metrics-matrix"), help="measured run folders")
    parser.add_argument("--evidence", type=Path, default=Path("docs/evidence"), help="evidence tree")
    options = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    export = options.dest if options.dest.is_absolute() else root / options.dest
    work = options.work if options.work.is_absolute() else root / options.work
    evidence = options.evidence if options.evidence.is_absolute() else root / options.evidence

    options.git_head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip() \
        if (root / ".git").exists() else "unknown (no git metadata)"
    if export.exists():
        shutil.rmtree(export)
    export.mkdir(parents=True)
    log: list[str] = []

    copy_tree(root / "src", export / "src", "SystemC sources", log)
    (export / "build").mkdir()
    for name in ("CMakeLists.txt", ".clang-format"):
        copy_file(root / name, export / "build" / name, "build config", log)
    for name in ("setup.ps1", "build-test.ps1"):
        copy_file(root / "scripts" / name, export / "build" / name, "build script", log)
    copy_tree(root / "tests", export / "tests", "test suite", log)
    collect_runs(work, export / "results", log)

    summary = export / "results" / "performance" / "summary"
    copy_tree(evidence / "metrics-matrix", summary, "metrics summary", log)
    remaining = export / "results" / "performance" / "evidence"
    remaining.mkdir(parents=True, exist_ok=True)
    for item in sorted(evidence.iterdir()) if evidence.is_dir() else []:
        if item.name != "metrics-matrix" and item.is_dir():
            copy_tree(item, remaining / item.name, f"evidence {item.name}", log)
        elif item.name != "metrics-matrix" and item.is_file():
            copy_file(item, remaining / item.name, "evidence file", log)

    for item in sorted(root.joinpath("docs").glob("*.md")):
        copy_file(item, export / "docs" / item.name, "design doc", log)

    total = write_manifest(export, log, options)
    print("\n".join(log))
    print(f"\nEXPORT DONE: {total} files -> {export}")


if __name__ == "__main__":
    main()
