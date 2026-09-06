"""Regenerate reviewable fixtures with fixed, independent random seeds."""
import json
import math
from pathlib import Path
import random

TEST_ROOT = Path(__file__).parent / "cases"
TEST_INT_MIN = -(2**31)
TEST_INT_MAX = 2**31 - 1
TEST_RANDOM_GROUPS = 10
TEST_RANDOM_TASKS = 64


def write_case(test_name, test_tasks, test_seed=None):
    (TEST_ROOT / f"{test_name}.txt").write_text(
        "".join(f"{test_a} {test_b}\n" for test_a, test_b in test_tasks), encoding="utf-8")
    (TEST_ROOT / f"{test_name}.expected.txt").write_text(
        "".join(f"{math.gcd(test_a, test_b)}\n" for test_a, test_b in test_tasks), encoding="utf-8")
    return {"name": test_name, "tasks": len(test_tasks), "seed": test_seed}


def main():
    TEST_ROOT.mkdir(exist_ok=True)
    test_values = [TEST_INT_MIN, TEST_INT_MIN + 1, -1, 0, 1, TEST_INT_MAX - 1, TEST_INT_MAX]
    test_manifest = [write_case("extreme_grid", [(test_a, test_b)
                                                for test_a in test_values for test_b in test_values])]
    test_fibonacci = [0, 1]
    while test_fibonacci[-1] <= TEST_INT_MAX:
        test_fibonacci.append(sum(test_fibonacci[-2:]))
    test_long = [(test_fibonacci[test_i], test_fibonacci[test_i - 1]) for test_i in range(30, 47)]
    test_manifest.append(write_case("fibonacci_consecutive", test_long))
    test_short = [(test_i, test_i) for test_i in range(1, 41)]
    test_manifest.append(write_case("short_baseline", test_short))
    test_mixed = test_short.copy()
    for test_position, test_task in zip((10, 21, 32), test_long[-3:]):
        test_mixed.insert(test_position, test_task)
    test_manifest.append(write_case("short_with_long", test_mixed))
    for test_group in range(TEST_RANDOM_GROUPS):
        test_seed = 2026090600 + test_group
        test_rng = random.Random(test_seed)
        test_tasks = [(test_rng.randint(TEST_INT_MIN, TEST_INT_MAX),
                       test_rng.randint(TEST_INT_MIN, TEST_INT_MAX)) for test_index in range(TEST_RANDOM_TASKS)]
        test_manifest.append(write_case(f"random_{test_group + 1:02d}", test_tasks, test_seed))
    (TEST_ROOT / "manifest.json").write_text(json.dumps(test_manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
