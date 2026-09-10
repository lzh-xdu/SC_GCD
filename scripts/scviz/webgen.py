"""Assemble the frontend data and inject it into the HTML template."""

import json
import os


def build_data(graph, trace):
    """Combine the structural graph and event trace into one frontend payload."""
    return {
        "meta": {
            "top": graph.top,
            "clock": graph.clock,
            "clock_period_ns": graph.clock_period_ns,
            "source_files": list(graph.files),
        },
        "modules": [
            {
                "id": m.id,
                "type": m.type,
                "ports": [
                    {"name": p.name, "direction": p.direction, "kind": p.kind, "label": p.label}
                    for p in m.ports
                ],
                "events": list(m.events),
            }
            for m in graph.modules
        ],
        "wires": [
            {
                "from": w.source, "to": w.target, "kind": w.kind,
                "from_port": w.source_port, "to_port": w.target_port, "label": w.label,
            }
            for w in graph.wires
        ],
        "trace": trace,
    }


def render(template_path, data):
    """Return the final HTML with ``__SCVIZ_DATA__`` replaced by JSON.

    ``</`` is escaped so the JSON can never terminate the script element.
    """
    with open(template_path, encoding="utf-8") as stream:
        template = stream.read()
    payload = json.dumps(data, ensure_ascii=False).replace("</", "<\\/")
    if "__SCVIZ_DATA__" not in template:
        raise ValueError("template is missing the __SCVIZ_DATA__ placeholder")
    return template.replace("__SCVIZ_DATA__", payload)


def write_output(out_path, html):
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as stream:
        stream.write(html)
