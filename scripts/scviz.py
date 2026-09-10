#!/usr/bin/env python3
"""Render a self-contained HTML page that animates SystemC module structure and cycles.

Usage (Stage 1 example)::

    python -B scripts/scviz.py \
        --src src/stage1/main.cpp src/stage1/parser.hpp src/stage1/parser.cpp \
               src/stage1/transform.hpp src/stage1/transform.cpp \
               src/stage1/compute.hpp src/stage1/compute.cpp \
               src/stage1/output.hpp src/stage1/output.cpp \
        --events build/debug/stage1-events.csv \
        --top System --clock-period 1.0 \
        --out web/stage1.html

The generated HTML is self-contained (no network, no CDN); open it directly in a
browser or serve the directory with ``python -m http.server``.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from scviz import events, sc_parser, webgen  # noqa: E402


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--src", nargs="+", required=True,
                        help="SystemC source files (.hpp/.h/.cpp), headers and implementations")
    parser.add_argument("--events", required=True, help="event CSV produced by the simulation")
    parser.add_argument("--top", help="top module name (default: auto-detected)")
    parser.add_argument("--clock-period", type=float, default=1.0, help="clock period in ns")
    parser.add_argument("--out", required=True, help="output HTML path")
    parser.add_argument("--title", default="SystemC Module Viewer", help="page title")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv if argv is not None else sys.argv[1:])
    for path in args.src:
        if not os.path.isfile(path):
            raise SystemExit(f"source file not found: {path}")
    if not os.path.isfile(args.events):
        raise SystemExit(f"event file not found: {args.events}")

    graph = sc_parser.parse(args.src, top=args.top, clock_period_ns=args.clock_period)
    event_module = {}
    for module in graph.modules:
        for event in module.events:
            event_module[event] = module.type
    trace = events.load_events(args.events, event_module=event_module)

    data = webgen.build_data(graph, trace, title=args.title)
    template = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scviz", "template.html")
    html = webgen.render(template, data)
    webgen.write_output(args.out, html)

    print(f"wrote {os.path.abspath(args.out)}")
    print(f"  top={graph.top} clock={graph.clock} ({graph.clock_period_ns} ns)")
    print(f"  modules={[m.id for m in graph.modules]}")
    print(f"  wires={len(graph.wires)}  trace_tasks={len(trace['tasks'])}  end_cycle={trace['end']}")


if __name__ == "__main__":
    main()
