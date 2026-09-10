"""Export the submission package into a temporary folder at the project root.

The export keeps the original project layout so the build scripts work
unmodified (CMakeLists.txt, src/, tests/, scripts/ side by side); the mapping
between the six submission categories and the folders, plus one-click build
and verification commands, are documented in the generated README.md:

    export/
    ├── README.md              structure, category mapping, one-click commands
    ├── CMakeLists.txt         build configuration (expects src/ tests/ beside it)
    ├── .clang-format
    ├── scripts/               setup.ps1, build-test.ps1, measurement scripts
    ├── src/                   all SystemC sources (common/model/stage1-4)
    ├── tests/                 test suite, case inputs, expected outputs;
    │   └── metrics-inputs/    generated scenario inputs (one per scenario)
    ├── results/
    │   ├── functional/        one GCD output file per measured run
    │   └── performance/
    │       ├── runs/          one stats CSV per measured run
    │       ├── summary/       aggregated tables, plots and timelines
    │       └── evidence/      remaining historical evidence trees
    └── docs/                  design documents (top-level *.md)

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
BUILD_SCRIPTS = ("setup.ps1", "build-test.ps1")
MEASUREMENT_SCRIPTS = ("run_metrics_matrix.py", "metrics_svg.py", "profile_model.py")


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


def collect_runs(work: Path, export: Path, log: list[str]):
    functional = export / "results" / "functional"
    performance = export / "results" / "performance" / "runs"
    inputs = export / "tests" / "metrics-inputs"
    functional.mkdir(parents=True, exist_ok=True)
    performance.mkdir(parents=True, exist_ok=True)
    inputs.mkdir(parents=True, exist_ok=True)
    seen_runs, seen_cases = set(), {}
    if not work.is_dir():
        log.append(f"MISSING  measured runs: {work} not found (rerun run_metrics_matrix.py)")
        return
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


def count_files(folder: Path) -> int:
    return sum(1 for item in folder.rglob("*") if item.is_file()) if folder.is_dir() else 0


def write_readme(export: Path, log: list[str], options) -> int:
    docs_count = count_files(export / "docs")
    mapping = [
        ("1. 所有 SystemC 源代码", "src/", count_files(export / "src")),
        ("2. 编译脚本或 CMakeLists.txt", "CMakeLists.txt、.clang-format、scripts/（setup.ps1、build-test.ps1）",
         count_files(export / "scripts") + 2),
        ("3. 测试输入", "tests/（用例输入与期望输出、验证脚本）；tests/metrics-inputs/（性能场景输入）",
         count_files(export / "tests")),
        ("4. 功能输出结果", "results/functional/（每个测量配置一份，每行一个最终 GCD）",
         count_files(export / "results" / "functional")),
        ("5. 性能统计结果",
         "results/performance/runs/（每配置统计 CSV）与 summary/（汇总表、图、时间线）；"
         "原始测量证据在 docs/evidence/（与文档链接一致）",
         count_files(export / "results" / "performance") + count_files(export / "docs" / "evidence")),
        ("6. 设计文档", f"docs/（{docs_count} 个 .md：最终设计、实测结果、AI 协作记录等；历史文档在 docs/archive/）",
         docs_count),
    ]
    total = sum(count for _, _, count in mapping)
    lines = [
        "# SC_GCD 提交导出（Submission Export）",
        "",
        f"生成时间：{datetime.now().isoformat(timespec='seconds')}　|　Git HEAD：`{options.git_head}`"
        + "　|　构建/测量方法见 `docs/metrics-results.md` §0",
        "",
        "## 1. 目录组织与提交要求映射",
        "",
        "| 提交要求 | 目录 | 文件数 |",
        "|---|---|---:|",
    ] + [f"| {requirement} | `{folder}` | {count} |" for requirement, folder, count in mapping]
    lines += [
        "",
        "```text",
        "export/",
        "├── README.md              本文件",
        "├── CMakeLists.txt         构建配置（与 src/、tests/ 同级，脚本可直接使用）",
        "├── .clang-format",
        "├── scripts/               setup.ps1（获取 SystemC）、build-test.ps1（构建+测试）、测量脚本",
        "├── src/                   SystemC 源代码：common/ model/(functional|timing|instrumentation) stage1~4",
        "├── tests/                 测试脚本、用例输入与期望输出；metrics-inputs/ 性能场景输入",
        "├── results/functional/    功能输出结果（文件名＝运行配置标签）",
        "├── results/performance/   runs/ 每配置统计 CSV；summary/ 汇总表与图",
        "├── docs/                  设计文档（final-design.md 最终设计、ai-log.md 协作记录、",
        "│                          metrics-results.md 实测报告；evidence/ 原始证据，archive/ 历史文档）",
        "```",
        "",
        "结果文件命名：`<model>-<case>-D<depth>-P<period>[-R<result_depth>[-W<window>-S<seed>]]-trace|notrace`，",
        "与 `docs/metrics-results.md` §0 的默认参数（D=2、R=2、W=8、SEED=7、P=1）对应。",
        "",
        "## 2. 一键构建与测试",
        "",
        "前置条件：g++（≥11，支持 C++17）、CMake ≥3.20、Ninja（Windows）、Python 3（运行测试）。",
        "首次使用需获取 SystemC 3.0.1 源码到 `third_party/systemc`（不入库，约 40 MB）。",
        "",
        "Windows（PowerShell，MinGW g++ 与 ninja 在 PATH 中）：",
        "",
        "```powershell",
        "powershell -ExecutionPolicy Bypass -File scripts/setup.ps1          # 首次：克隆 SystemC 3.0.1",
        "powershell -ExecutionPolicy Bypass -File scripts/build-test.ps1    # 配置+构建+全部测试（Release）",
        "```",
        "",
        "Linux / WSL：",
        "",
        "```bash",
        "git clone --depth 1 --branch 3.0.1 https://github.com/accellera-official/systemc.git third_party/systemc",
        "cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++",
        "cmake --build build -j$(nproc)          # 或 --target stage1_gcd stage2_gcd stage3_gcd stage3_window_gcd stage4_gcd",
        "ctest --test-dir build --output-on-failure   # 预期 25/25 通过",
        "```",
        "",
        "## 3. 一键复现功能输出与性能统计",
        "",
        "以下命令直接用导出的场景输入运行模型，并与导出的结果逐字节比对；",
        "统计 CSV 只含模拟指标（无主机耗时），同源码同输入应为逐字节一致：",
        "",
        "```bash",
        "# 作业1：混合负载（stage1，D=2，P=1）",
        "./build/stage1_gcd tests/metrics-inputs/mixed.txt /tmp/out.csv /tmp/stats.csv 2 - 1 1000000000",
        "diff /tmp/out.csv   results/functional/stage1-mixed-D2-P1-notrace.output.txt",
        "diff /tmp/stats.csv results/performance/runs/stage1-mixed-D2-P1-notrace.stats.csv",
        "",
        "# 作业3：长短偏斜负载（stage3_window，D=2，P=1，R=2，W=8，种子7）",
        "./build/stage3_window_gcd tests/metrics-inputs/skew.txt /tmp/out.csv /tmp/stats.csv 2 - 1 1000000000 2 8 7",
        "diff /tmp/out.csv   results/functional/stage3_window-skew-D2-P1-R2-W8-S7-notrace.output.txt",
        "diff /tmp/stats.csv results/performance/runs/stage3_window-skew-D2-P1-R2-W8-S7-notrace.stats.csv",
        "```",
        "",
        "重新生成完整测量数据（可选，约 4 分钟）：",
        "",
        "```bash",
        "python3 scripts/run_metrics_matrix.py   # 75 次运行+160 次主机测量 → docs/evidence/metrics-matrix/",
        "```",
        "",
        "各阶段命令行：`INPUT OUTPUT STATS [DEPTH=2] [EVENTS.csv|-] [OUTPUT_PERIOD=1] [MAX_CYCLES] "
        "[RESULT_DEPTH=2] [WINDOW=8] [SEED=1]`（取各阶段支持的前缀参数）。",
        "",
        "## 4. 复制日志（异常项需关注）",
        "",
    ]
    notable = [entry for entry in log if not entry.startswith("OK")]
    lines += [f"- {entry}" for entry in notable] or ["- 无缺失或告警"]
    lines += ["", f"总计导出 {total} 个文件。", ""]
    (export / "README.md").write_text("\n".join(lines), encoding="utf-8")
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
    for name in ("CMakeLists.txt", ".clang-format"):
        copy_file(root / name, export / name, "build config", log)
    for name in BUILD_SCRIPTS + MEASUREMENT_SCRIPTS:
        copy_file(root / "scripts" / name, export / "scripts" / name, "script", log)
    copy_tree(root / "tests", export / "tests", "test suite", log)
    collect_runs(work, export, log)

    stats = export / "results" / "performance"
    copy_tree(evidence / "metrics-matrix", stats / "summary", "metrics summary", log)
    # Full evidence tree keeps every docs/ link (and archive/ links) valid in
    # the export; metrics-matrix is intentionally also exposed as summary/.
    copy_tree(evidence, export / "docs" / "evidence", "evidence tree", log)

    for item in sorted(root.joinpath("docs").glob("*.md")):
        copy_file(item, export / "docs" / item.name, "design doc", log)
    copy_tree(root / "docs" / "archive", export / "docs" / "archive", "archived design docs", log)

    total = write_readme(export, log, options)
    print("\n".join(entry for entry in log if not entry.startswith("OK"))
          or "no missing files or warnings")
    print(f"EXPORT DONE: {total} files -> {export}")


if __name__ == "__main__":
    main()
