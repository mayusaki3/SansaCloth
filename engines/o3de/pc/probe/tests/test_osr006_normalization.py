"""OSR-006 regression guard for the validation-only O3DE normalizer.

Run: python -m unittest discover -s engines/o3de/pc/probe/tests -p "test_*.py"
This checks source shape and IEEE-754 examples; full parity requires O3DE Editor.
"""
import math
import re
import unittest
from pathlib import Path

SOURCE = (
    Path(__file__).resolve().parents[1]
    / "Gem/SansaClothBackendProbeValidation/Source"
    / "SansaClothBackendProbeValidationSystemComponent.cpp"
)


class Osr006NormalizationTests(unittest.TestCase):
    def test_cpp_normalizer_uses_reciprocal_multiply(self):
        source = SOURCE.read_text(encoding="utf-8")
        match = re.search(r"V normalized\(\) const\s*\{([^}]*?return[^;]*;)", source, re.S)
        self.assertIsNotNone(match, "OSR-006 validation normalizer not found")
        body = match.group(1)
        self.assertIn("(*this) * (1.0 / n)", body)
        self.assertNotIn("(*this)/n", body)

    def test_reference_rounding_example(self):
        # Captured SR-002-C0-G0 stable_id=11: raw_normal.y and normal_length.
        y = float.fromhex("0x1.5d867c3ece2a8p-13")
        length = float.fromhex("0x1.6b95bd6b2c615p-13")
        self.assertEqual((y * (1.0 / length)).hex(), "0x1.ec3388540919fp-1")
        self.assertEqual((y / length).hex(), "0x1.ec3388540919ep-1")

    def test_zero_and_nonfinite_guards_remain(self):
        source = SOURCE.read_text(encoding="utf-8")
        self.assertIn("n > 0 && std::isfinite(n)", source)


if __name__ == "__main__":
    unittest.main()
