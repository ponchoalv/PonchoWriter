"""Optional tooling checks: python3 -m unittest discover -s tests -p test_perf_compare.py"""
import contextlib
import copy
import runpy
import io
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
compare = runpy.run_path(str(ROOT / "bin/perf-compare"))["compare"]


class CompareTest(unittest.TestCase):
    def setUp(self):
        self.report = json.loads((ROOT / "docs/performance/metal-baseline.json").read_text())

    def check(self, before, after):
        with contextlib.redirect_stdout(io.StringIO()):
            compare(before, after)

    def test_matching_reports(self):
        self.check(self.report, copy.deepcopy(self.report))

    def test_incomplete(self):
        other = copy.deepcopy(self.report)
        other["complete"] = False
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_monitor_changed(self):
        other = copy.deepcopy(self.report)
        other["screen_refresh_hz"] = 60
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_harness_changed(self):
        other = copy.deepcopy(self.report)
        other["benchmark_configuration_sha"] = "different"
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_input_changed(self):
        other = copy.deepcopy(self.report)
        other["results"][0]["source_characters"] += 1
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_missing_fixture(self):
        other = copy.deepcopy(self.report)
        other["results"].pop()
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_duplicate_fixture(self):
        other = copy.deepcopy(self.report)
        other["results"].append(copy.deepcopy(other["results"][0]))
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_unknown_fixture(self):
        other = copy.deepcopy(self.report)
        other["results"][0]["fixture"] = "unexpected"
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_missing_scroll_mode(self):
        other = copy.deepcopy(self.report)
        other["results"][0]["scrolling"].pop()
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_no_scrolling(self):
        other = copy.deepcopy(self.report)
        other["results"][0]["scrolling"] = []
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_duplicate_scroll_mode(self):
        other = copy.deepcopy(self.report)
        scrolling = other["results"][0]["scrolling"]
        scrolling[1]["mode"] = scrolling[0]["mode"]
        with self.assertRaises(ValueError):
            self.check(self.report, other)

    def test_reordered_scrolling(self):
        other = copy.deepcopy(self.report)
        other["results"][0]["scrolling"].reverse()
        self.check(self.report, other)

    def test_schema_changed(self):
        other = copy.deepcopy(self.report)
        other["schema"] += 1
        with self.assertRaises(ValueError):
            self.check(self.report, other)


if __name__ == "__main__":
    unittest.main()
