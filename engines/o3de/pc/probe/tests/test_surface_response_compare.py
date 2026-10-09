"""Host-Python regression tests; run with python -m unittest discover -s engines/o3de/pc/probe/tests."""
import copy
import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).resolve().parents[1] / "Editor/Scripts/SansaCloth/sansacloth_surface_response_compare.py"
spec = importlib.util.spec_from_file_location("osr_compare", MODULE)
compare_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare_module)


def sample():
    points = []
    for i in range(147):
        point = {"stable_id": i, "support": "Anchor" if i % 21 in (0, 20) else "Unsupported",
                 "contact": True, "separation_m": 0.0}
        point.update({key: [0.0, 0.0, 0.0] for key in compare_module.VECTORS})
        points.append(point)
    return {"format": compare_module.FORMAT, "case_id": "SR-001-C0-G0",
            "profile": {"support_layout": "BothEdges"}, "control_points": points,
            "aggregate": {"control_point_count": 147, "support_count": 14, "contact_count": 147}}


class ComparatorTests(unittest.TestCase):
    def test_identity(self):
        source = sample()
        self.assertEqual(compare_module.compare(source, copy.deepcopy(source),
                         absolute_tolerance_m=0.0, normal_tolerance=0.0)["final_position_m"], 0.0)

    def test_stable_id_order_independent(self):
        source = sample()
        actual = copy.deepcopy(source)
        actual["control_points"].reverse()
        compare_module.compare(source, actual, absolute_tolerance_m=0, normal_tolerance=0)

    def test_numeric_mismatch_rejected(self):
        source = sample()
        actual = copy.deepcopy(source)
        actual["control_points"][10]["final_position_m"][1] = 0.001
        with self.assertRaises(ValueError):
            compare_module.compare(source, actual, absolute_tolerance_m=1e-5, normal_tolerance=1e-5)

    def test_support_mismatch_rejected(self):
        source = sample()
        actual = copy.deepcopy(source)
        actual["control_points"][1]["support"] = "Anchor"
        actual["aggregate"]["support_count"] += 1
        with self.assertRaises(ValueError):
            compare_module.compare(source, actual, absolute_tolerance_m=0, normal_tolerance=0)

    def test_missing_point_rejected(self):
        source = sample()
        actual = copy.deepcopy(source)
        actual["control_points"].pop()
        with self.assertRaises(ValueError):
            compare_module.compare(source, actual, absolute_tolerance_m=0, normal_tolerance=0)

    def test_nonfinite_rejected(self):
        source = sample()
        actual = copy.deepcopy(source)
        actual["control_points"][1]["separation_m"] = float("nan")
        with self.assertRaises(ValueError):
            compare_module.compare(source, actual, absolute_tolerance_m=0, normal_tolerance=0)


if __name__ == "__main__":
    unittest.main()
