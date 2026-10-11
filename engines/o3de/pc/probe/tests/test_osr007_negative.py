"""OSR-007 host regression: every injected mutation must fail comparison."""
import copy
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "Editor/Scripts/SansaCloth/sansacloth_surface_response_compare.py"
spec = importlib.util.spec_from_file_location("osr007_compare", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fixture():
    points = []
    for i in range(147):
        point = {"stable_id": i, "support": "Anchor" if i % 21 in (0, 20) else "Unsupported",
                 "contact": True, "separation_m": 0.0}
        point.update({name: [0.0, 0.0, 0.0] for name in module.VECTORS})
        points.append(point)
    return {"format": module.FORMAT, "case_id": "SR-001-C0-G0",
            "profile": {"support_layout": "BothEdges"}, "control_points": points,
            "aggregate": {"control_point_count": 147, "support_count": 14, "contact_count": 147}}


class NegativeGateTests(unittest.TestCase):
    def test_each_numeric_field_rejects_mutation(self):
        expected = fixture()
        for field in module.VECTORS + module.SCALARS:
            with self.subTest(field=field):
                actual = copy.deepcopy(expected)
                if field in module.VECTORS:
                    actual["control_points"][42][field][1] = 0.001
                else:
                    actual["control_points"][42][field] = 0.001
                with self.assertRaises(ValueError):
                    module.compare(expected, actual, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)

    def test_contact_mismatch_with_consistent_aggregate_rejected(self):
        expected = fixture()
        actual = copy.deepcopy(expected)
        actual["control_points"][42]["contact"] = False
        actual["aggregate"]["contact_count"] -= 1
        with self.assertRaises(ValueError):
            module.compare(expected, actual, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)

    def test_ack_only_payload_rejected(self):
        with self.assertRaises(ValueError):
            module.compare(fixture(), {"ack": "accepted"}, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)

    def test_profile_mismatch_rejected(self):
        expected = fixture()
        actual = copy.deepcopy(expected)
        actual["profile"]["support_layout"] = "OneEdge"
        with self.assertRaises(ValueError):
            module.compare(expected, actual, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)


if __name__ == "__main__":
    unittest.main()
