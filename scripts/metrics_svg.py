"""Minimal dependency-free SVG charts for the metrics evidence reports.

Only what the metrics matrix needs: grouped bars (with optional Q1-Q3
whiskers and log scale) and simple line plots with markers. Output files
are standalone SVG documents that render in browsers and VS Code previews.
"""
from __future__ import annotations

import math
from pathlib import Path

PALETTE = ["#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c", "#0891b2"]
FONT = "font-family='sans-serif'"


def _esc(text: str) -> str:
    return str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def _nice_ticks(low: float, high: float, target: int = 5) -> list[float]:
    if high <= low:
        return [low]
    raw = (high - low) / max(1, target)
    magnitude = 10 ** math.floor(math.log10(raw))
    for factor in (1, 2, 5, 10):
        step = factor * magnitude
        if step >= raw:
            break
    ticks, value = [], math.ceil(low / step) * step
    while value <= high + step * 1e-9:
        ticks.append(round(value, 10))
        value += step
    return ticks


def _fmt(value: float) -> str:
    if value == 0:
        return "0"
    if abs(value) >= 1000:
        return f"{value:,.0f}"
    if abs(value) >= 10:
        return f"{value:.1f}".rstrip("0").rstrip(".")
    return f"{value:.3g}"


def _nice_step(raw: float) -> float:
    """Smallest 1/2/5-ladder step that covers the raw spacing."""
    if raw <= 0:
        return 1.0
    magnitude = 10 ** math.floor(math.log10(raw))
    for factor in (1, 2, 5, 10):
        step = factor * magnitude
        if step >= raw:
            return step
    return 10 * magnitude


def _snap_axis(low: float, high: float, target: int = 5):
    """Expand [low, high] to round multiples of a nice tick step.

    Returns (low_nice, high_nice, step) so gridlines land on values that are
    readable at the zoom level the data actually spans; never returns a
    negative lower bound for non-negative data.
    """
    if not math.isfinite(low) or not math.isfinite(high) or high < low:
        return 0.0, 1.0, 0.2
    if high == low:
        pad = abs(low) * 0.1 if low else 0.5
        low, high = low - pad, high + pad
    step = _nice_step((high - low) / max(1, target))
    low_nice = math.floor(low / step) * step
    high_nice = math.ceil(high / step) * step
    if high_nice <= low_nice:
        high_nice = low_nice + step
    return round(low_nice, 10), round(high_nice, 10), step


def _tick_label(value: float, step: float) -> str:
    """Tick label with decimals matched to the step so zoomed axes stay readable."""
    if value == 0:
        return "0"
    decimals = max(0, -math.floor(math.log10(step)))
    text = f"{value:.{decimals}f}"
    if abs(value) >= 1000 and decimals == 0:
        return f"{value:,.0f}"
    return text


class _Canvas:
    def __init__(self, width: int, height: int):
        self.width, self.height = width, height
        self.parts = [
            f"<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}' "
            f"viewBox='0 0 {width} {height}'>",
            f"<rect width='{width}' height='{height}' fill='white'/>",
        ]

    def line(self, x1, y1, x2, y2, stroke="#6b7280", width=1, dash=None):
        extra = f" stroke-dasharray='{dash}'" if dash else ""
        self.parts.append(
            f"<line x1='{x1:.1f}' y1='{y1:.1f}' x2='{x2:.1f}' y2='{y2:.1f}' "
            f"stroke='{stroke}' stroke-width='{width}'{extra}/>")

    def rect(self, x, y, w, h, fill, stroke="none"):
        self.parts.append(
            f"<rect x='{x:.1f}' y='{y:.1f}' width='{max(w, 0):.1f}' height='{max(h, 0):.1f}' "
            f"fill='{fill}' stroke='{stroke}'/>")

    def text(self, x, y, content, size=12, anchor="middle", fill="#111827", bold=False, rotate=None):
        transform = f" transform='rotate({rotate} {x:.1f} {y:.1f})'" if rotate is not None else ""
        weight = " font-weight='bold'" if bold else ""
        self.parts.append(
            f"<text x='{x:.1f}' y='{y:.1f}' font-size='{size}' {FONT} text-anchor='{anchor}' "
            f"fill='{fill}'{weight}{transform}>{_esc(content)}</text>")

    def save(self, path: Path):
        self.parts.append("</svg>")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("\n".join(self.parts) + "\n", encoding="utf-8")


class _Axis:
    """Maps data coordinates into canvas pixels with optional log-y."""

    def __init__(self, left, top, right, bottom, x0, x1, y0, y1, log_y=False, step=None):
        self.left, self.top, self.right, self.bottom = left, top, right, bottom
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        self.log_y = log_y
        self.step = step
        if log_y:
            self.y0 = math.log10(max(y0, 1e-12))
            self.y1 = math.log10(max(y1, y0 * 1.000001))

    def px(self, x):
        span = (self.x1 - self.x0) or 1.0
        return self.left + (x - self.x0) / span * (self.right - self.left)

    def py(self, y):
        if self.log_y:
            y = math.log10(max(y, 1e-12))
        span = (self.y1 - self.y0) or 1.0
        return self.bottom - (y - self.y0) / span * (self.bottom - self.top)

    def y_ticks(self, target=5):
        if self.log_y:
            low, high = 10 ** self.y0, 10 ** self.y1
            ticks = [t for t in _nice_ticks(low, high, target) if low <= t <= high]
            if len(ticks) < 2:
                ticks = [low, high]
            return ticks
        if self.step:
            count = int(round((self.y1 - self.y0) / self.step + 1e-9))
            return [round(self.y0 + index * self.step, 10) for index in range(count + 1)]
        return _nice_ticks(self.y0, self.y1, target)


def _draw_axes(canvas, axis, xlabel, ylabel, x_labels):
    canvas.line(axis.left, axis.bottom, axis.right, axis.bottom)
    canvas.line(axis.left, axis.top, axis.left, axis.bottom)
    for tick in axis.y_ticks():
        y = axis.py(tick)
        canvas.line(axis.left, y, axis.right, y, stroke="#e5e7eb")
        label = _tick_label(tick, axis.step) if axis.step else _fmt(tick)
        canvas.text(axis.left - 6, y + 4, label, size=11, anchor="end", fill="#374151")
    if x_labels:
        for x, label in x_labels:
            canvas.text(axis.px(x), axis.bottom + 16, label, size=11, fill="#374151")
    if xlabel:
        canvas.text((axis.left + axis.right) / 2, axis.bottom + 40, xlabel, size=13, bold=True)
    if ylabel:
        canvas.text(16, (axis.top + axis.bottom) / 2, ylabel, size=13, bold=True, rotate=-90)


def _legend(canvas, series, top, left):
    x = left
    for index, name in enumerate(series):
        color = PALETTE[index % len(PALETTE)]
        canvas.rect(x, top - 9, 12, 12, color)
        canvas.text(x + 16, top + 1, name, size=12, anchor="start")
        x += 28 + sum(14 if ord(char) > 0x2E7F else 7.5 for char in str(name))


def grouped_bar_chart(path, categories, series, *, ylabel, title, xlabel=None, log_y=False,
                      errors=None, height=430, width=880, label_values=True):
    """Vertical grouped bars; one group per category, one bar per series."""
    canvas = _Canvas(width, height)
    axis = _Axis(72, 64, width - 20, height - 58, -0.5, len(categories) - 0.5, 0, 1, log_y)
    values = [v for vs in series.values() for v in vs if v > 0]
    if errors:
        values += [hi for pairs in errors.values() for _, hi in pairs if hi > 0]
    top = max(values) if values else 1.0
    axis.y1 = top * (1.18 if log_y else 1.12)
    axis.y0 = min(min(values), 0) if values and not log_y else (min(values) * 0.9 if values and log_y else 0)
    if log_y and axis.y0 <= 0:
        axis.y0 = top / 1000.0
    if log_y:
        _Axis.__init__(axis, axis.left, axis.top, axis.right, axis.bottom, axis.x0, axis.x1, axis.y0, axis.y1, True)
    _draw_axes(canvas, axis, xlabel, ylabel, [(i, c) for i, c in enumerate(categories)])
    group_width = (axis.right - axis.left) / max(1, len(categories))
    bar_width = min(46, group_width * 0.8 / max(1, len(series)))
    for column, (name, ys) in enumerate(series.items()):
        color = PALETTE[column % len(PALETTE)]
        for row, value in enumerate(ys):
            cx = axis.px(row)
            x = cx - bar_width * len(series) / 2 + column * bar_width
            y = axis.py(value)
            canvas.rect(x + 1, y, bar_width - 2, axis.bottom - y, color)
            if errors and name in errors:
                lo, hi = errors[name][row]
                for level, marker in ((lo, "lo"), (hi, "hi")):
                    ey = axis.py(level)
                    canvas.line(x + 1, ey, x + bar_width - 1, ey, stroke="#111827", width=1.4)
                mid = x + bar_width / 2 - 0.5
                canvas.line(mid, axis.py(lo), mid, axis.py(hi), stroke="#111827", width=1.4)
            if label_values and value > 0:
                canvas.text(x + bar_width / 2, y - 4, _fmt(value), size=10, fill="#374151")
    _legend(canvas, series, 30, 72)
    canvas.text(width / 2, 18, title, size=15, bold=True)
    canvas.save(Path(path))


def line_chart(path, xs, series, *, xlabel, ylabel, title, log_y=False, height=430, width=880, dashed=(),
               y_range=None):
    """One polyline per series sharing the same x values.

    The linear y-axis snaps to the data span (rounded to a 1/2/5 tick step)
    so close values stay distinguishable; pass y_range=(low, high) to pin it.
    Names listed in ``dashed`` draw with a dashed stroke.
    """
    canvas = _Canvas(width, height)
    values = [v for vs in series.values() for v in vs if v is not None and math.isfinite(v)]
    if log_y:
        positive = [v for v in values if v > 0]
        low, high = (min(positive), max(positive)) if positive else (1.0, 10.0)
        axis = _Axis(72, 64, width - 20, height - 58, min(xs), max(xs),
                     low * 0.85, high * 1.15, log_y=True)
    elif y_range is not None:
        axis = _Axis(72, 64, width - 20, height - 58, min(xs), max(xs),
                     y_range[0], y_range[1], step=_nice_step((y_range[1] - y_range[0]) / 5))
    else:
        span = (max(values) - min(values)) if values else 0.0
        low = min(values) - span * 0.12 if values else 0.0
        high = max(values) + span * 0.18 if values else 1.0
        low_nice, high_nice, step = _snap_axis(low, high)
        if values and min(values) >= 0 and low_nice < 0:
            low_nice = 0.0  # Never suggest negative storage/occupancy.
        axis = _Axis(72, 64, width - 20, height - 58, min(xs), max(xs), low_nice, high_nice, step=step)
    _draw_axes(canvas, axis, xlabel, ylabel, [(x, _fmt(x)) for x in xs])
    label_step = axis.step if axis.step else (axis.y1 - axis.y0) / 5
    for column, (name, ys) in enumerate(series.items()):
        color = PALETTE[column % len(PALETTE)]
        pattern = "7 5" if name in dashed else None
        points = [(axis.px(x), axis.py(y)) for x, y in zip(xs, ys)]
        if len(points) >= 2:
            for (x1, y1), (x2, y2) in zip(points, points[1:]):
                canvas.line(x1, y1, x2, y2, stroke=color, width=2.2, dash=pattern)
        for x, y in points:
            canvas.parts.append(
                f"<circle cx='{x:.1f}' cy='{y:.1f}' r='4' fill='{color}'/>")
        for x, y, value in zip(xs, ys, ys):
            canvas.text(axis.px(x), axis.py(y) - 8, _tick_label(value, label_step), size=10, fill="#374151")
    _legend(canvas, series, 30, 72)
    canvas.text(width / 2, 18, title, size=15, bold=True)
    canvas.save(Path(path))
