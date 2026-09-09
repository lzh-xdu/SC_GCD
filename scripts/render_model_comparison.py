"""Render measured model comparison tables and a static figure; never invent samples."""
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "docs/evidence/model-comparison"
LABELS = {"dense_zero": "连续零延迟", "long_compute": "长计算", "mixed": "随机混合",
          "slow_sink": "慢输出", "sparse_wait": "稀疏长等待", "narrow_window": "窄窗口",
          "dense_trace": "短任务＋Trace", "mixed_trace": "混合任务＋Trace"}


def main():
    rows = json.loads((REPORT / "summary.json").read_text(encoding="utf-8"))
    index = {(row["case"], row["model"]): row for row in rows}
    plt.rcParams.update({"font.family": "Microsoft YaHei", "axes.unicode_minus": False, "font.size": 10})
    figure, axes = plt.subplots(2, 2, figsize=(14, 10), constrained_layout=True)
    positions = np.arange(len(LABELS))
    for axis, metric, title, scale in [
        (axes[0, 0], "model_ms", "模型执行耗时：中位数与四分位范围", 1),
        (axes[0, 1], "process_ms", "整个进程耗时：包含启动", 1),
        (axes[1, 0], "delta_cycles", "内核 delta 轮次（不是进程唤醒总数）", 1),
        (axes[1, 1], "peak_working_set_bytes", "进程峰值工作集", 1048576),
    ]:
        for offset, model, color, label in [(-0.18, "clock", "#667085", "时钟驱动"),
                                            (0.18, "event", "#007f86", "事件驱动")]:
            values = [index[(case, model)][metric] / scale for case in LABELS]
            errors = None
            if metric.endswith("ms"):
                errors = [[value - index[(case, model)][metric + "_q1"] for case, value in zip(LABELS, values)],
                          [index[(case, model)][metric + "_q3"] - value for case, value in zip(LABELS, values)]]
            axis.barh(positions + offset, values, height=0.34, color=color, label=label, xerr=errors)
        axis.set_yticks(positions, list(LABELS.values()))
        axis.invert_yaxis()
        axis.set_title(title, loc="left", fontweight="bold")
        if metric != "peak_working_set_bytes":
            axis.set_xscale("log")
        axis.set_xlabel("MiB" if scale != 1 else "ms（对数轴）" if metric.endswith("ms") else "轮次（对数轴）")
        axis.grid(axis="x", alpha=0.2)
        axis.set_axisbelow(True)
    axes[0, 0].legend(loc="lower right")
    figure.suptitle("同结果、同时序；事件驱动的收益取决于空闲区间与 Trace 成本", fontsize=16)
    figure.savefig(REPORT / "comparison.png", dpi=160)
    plt.close(figure)


if __name__ == "__main__":
    main()
