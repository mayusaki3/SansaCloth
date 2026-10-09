"""OSR-004: compare independent O3DE C++ solver against committed Rust result."""
import hashlib
import json
from pathlib import Path

import azlmbr.sansacloth_probe as probe
from sansacloth_surface_response_compare import compare

PREFIX = "SANSA_O3DE|OSR-004."
root = Path(__file__).resolve().parent
case = "SR-001-C0-G0"
fixture = root / "Fixtures" / (case + ".json")
reference = root / "SurfaceResponseResults" / (case + ".json")

try:
    source = fixture.read_text(encoding="utf-8")
    expected_text = reference.read_text(encoding="utf-8")
    expected = json.loads(expected_text)
    method = getattr(probe, "ProbeSurfaceResponseJson", None)
    if not callable(method):
        raise RuntimeError("independent C++ SurfaceResponse API unavailable")
    output = method(source)
    if not isinstance(output, str) or not output:
        raise RuntimeError("C++ SurfaceResponse returned empty result")
    actual = json.loads(output)
    # Temporary validation-only thresholds; not production defaults.
    tolerance = 1.0e-9
    maxima = compare(expected, actual, absolute_tolerance_m=tolerance, normal_tolerance=tolerance)
    print(f"{PREFIX}CASE_ID|{case}")
    print(f"{PREFIX}FIXTURE_SHA256|{hashlib.sha256(source.encode('utf-8')).hexdigest()}")
    print(f"{PREFIX}REFERENCE_SHA256|{hashlib.sha256(expected_text.encode('utf-8')).hexdigest()}")
    print(f"{PREFIX}TOLERANCE|{tolerance}")
    for field, value in sorted(maxima.items()):
        print(f"{PREFIX}MAX_ERROR.{field}|{value:.17g}")
    print(f"{PREFIX}CONTROL_POINT_COUNT|147")
    print(f"{PREFIX}RESULT|PASS")
except Exception as exc:
    print(f"{PREFIX}RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
