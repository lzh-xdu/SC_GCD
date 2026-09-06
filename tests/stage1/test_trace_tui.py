"""Semantic and keyboard regression tests; run with unittest discovery."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

testRoot = Path(__file__).resolve().parents[2]
testSpec = importlib.util.spec_from_file_location("trace_tui", testRoot / "scripts/trace_tui.py")
testModule = importlib.util.module_from_spec(testSpec)
testSpec.loader.exec_module(testModule)


class TestTraceViewer(unittest.TestCase):
    def load(self, testCycles, testLatency=6):
        with tempfile.TemporaryDirectory() as testDirectory:
            testPath = Path(testDirectory) / "events.csv"
            testPath.write_text("id,event,cycle,a,b,value,latency\n" + "".join(
                f"0,{testName},{testCycle},48,18,6,{testLatency if testName == 'compute_accept' else 0}\n"
                for testName, testCycle in zip(testModule.EVENTS, testCycles)), encoding="utf-8")
            return testModule.Trace(testPath)

    def test_half_open_intervals_and_metrics(self):
        testTrace = self.load([1, 2, 4, 5, 11, 11, 12])
        self.assertEqual([testTrace.state(0, testCycle) for testCycle in [0, 1, 2, 4, 5, 10, 11, 12, 13]],
                         list(".FTQCCO*."))
        self.assertEqual(testTrace.metrics(), (1, 6, 0, 11))

    def test_zero_latency_and_backpressure(self):
        testTrace = self.load([1, 2, 8, 9, 9, 14, 20], 0)
        self.assertEqual(testTrace.metrics(), (1, 0, 5, 19))
        self.assertEqual(testTrace.state(0, 9), "B")
        self.assertEqual(testTrace.duration(0, "transform_accept", "transform_emit"), 6)

    def test_partial_and_empty(self):
        testTrace = self.load([1, 2, 4, 5])
        self.assertEqual(testTrace.metrics()[0], 0)
        self.assertEqual(testTrace.state(0, 5), "C")
        self.assertIn("0/1 complete", testModule.View(testTrace).render())
        self.assertIn("Empty trace", testModule.View(self.load([])).render())

    def test_invalid_order(self):
        with self.assertRaisesRegex(ValueError, "backwards"):
            self.load([1, 2, 1])

    def test_navigation_and_render(self):
        testView = testModule.View(self.load([1, 2, 4, 5, 11, 11, 12]))
        testView.handle("n")
        self.assertEqual(testView.cycle, 1)
        testView.handle("p")
        self.assertEqual(testView.cycle, 1)
        testView.handle("l")
        testStart = testView.start
        testView.handle("")
        self.assertEqual(testView.start, testStart)
        testView.handle("0")
        testView.handle("-")
        self.assertEqual(testView.scale, 2)
        testView.handle("+")
        self.assertEqual(testView.scale, 1)
        testView.handle(" ")
        self.assertTrue(testView.playing)
        self.assertFalse(testView.handle("q"))
        self.assertIn("\033[32mC", testView.render(color=True))


if __name__ == "__main__":
    unittest.main()
