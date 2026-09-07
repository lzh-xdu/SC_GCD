"""Independent reconstruction from externally recorded task edges."""
import math


def check_statistics(test_rows, test_metrics, test_units):
    test_names = ("parser_send", "compute_accept", "compute_complete", "compute_emit", "output")
    test_events = {test_name: {} for test_name in test_names}
    test_unit_by_id = {}
    for test_row in test_rows:
        if test_row["event"] in test_events:
            test_events[test_row["event"]][test_row["id"]] = test_row["cycle"]
        if test_row["event"] == "compute_unit":
            test_unit_by_id[test_row["id"]] = test_row["a"]
    test_samples = {test_name: [] for test_name in ("precompute", "compute", "result_wait", "delivery", "end_to_end")}
    for test_id in test_events["output"]:
        test_edges = [test_events[test_name][test_id] for test_name in test_names]
        test_parts = [test_edges[test_i + 1] - test_edges[test_i] for test_i in range(4)]
        assert all(test_part >= 0 for test_part in test_parts)
        assert sum(test_parts) == test_edges[-1] - test_edges[0]
        for test_name, test_value in zip(test_samples, test_parts + [sum(test_parts)]):
            test_samples[test_name].append(test_value)
    assert test_metrics["task_latency_count"] == len(test_events["output"])
    for test_name, test_values in test_samples.items():
        test_count = len(test_values)
        test_expected = {
            "sum": sum(test_values), "mean": sum(test_values) / test_count if test_count else 0,
            "max": max(test_values, default=0),
            "p95": sorted(test_values)[math.ceil(0.95 * test_count) - 1] if test_count else 0,
        }
        for test_kind, test_value in test_expected.items():
            test_key = f"task_latency_{test_name}_{test_kind}_cycles"
            assert math.isclose(test_metrics[test_key], test_value, rel_tol=1e-10, abs_tol=1e-9), test_key
    for test_unit in range(test_units):
        test_occupied = set()
        for test_id, test_start in test_events["compute_accept"].items():
            if test_unit_by_id.get(test_id, 0) == test_unit:
                test_occupied.update(range(test_start, test_events["compute_emit"][test_id] + 1))
        test_idle = int(test_metrics["cycles"]) - len(test_occupied)
        assert test_metrics[f"compute{test_unit}_idle_no_input_cycles"] == test_idle
