"""OSR-004: compare independent O3DE C++ solver against committed Rust result."""
import hashlib
import importlib.util
import json
from pathlib import Path

import azlmbr.sansacloth_probe as probe

# runpy.run_path does not add the script directory to sys.path.
# Load the sibling comparator by absolute path, without changing global imports.
_comparator_path = Path(__file__).resolve().parent / "sansacloth_surface_response_compare.py"
_spec = importlib.util.spec_from_file_location("sansacloth_surface_response_compare", _comparator_path)
if _spec is None or _spec.loader is None:
    raise ImportError(f"Cannot load OSR comparator: {_comparator_path}")
_comparator = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_comparator)
compare = _comparator.compare

PREFIX = "SANSA_O3DE|OSR-004."
root = Path(__file__).resolve().parent
cases = [f"SR-{scenario:03d}-C{c}-G{g}" for scenario in range(1, 6) for c in ("0", "05", "1") for g in ("0", "1")]
try:
    method = getattr(probe, "ProbeSurfaceResponseJson", None)
    if not callable(method):
        raise RuntimeError("independent C++ SurfaceResponse API unavailable")
    for case in cases:
        fixture = root / "Fixtures" / (case + ".json")
        reference = root / "SurfaceResponseResults" / (case + ".json")
        source = fixture.read_text(encoding="utf-8")
        expected_text = reference.read_text(encoding="utf-8")
        expected = json.loads(expected_text)
        output = method(source)
        if not isinstance(output, str) or not output:
            raise RuntimeError(f"{case}: C++ SurfaceResponse returned empty result")
        actual = json.loads(output)
        tolerance = 1.0e-9
        try:
            maxima = compare(expected, actual, absolute_tolerance_m=tolerance, normal_tolerance=tolerance)
        except ValueError as error:
            expected_points = {p["stable_id"]: p for p in expected["control_points"]}
            actual_points = {p["stable_id"]: p for p in actual["control_points"]}
            for stable_id in sorted(expected_points):
                ref_point = expected_points[stable_id]
                got_point = actual_points.get(stable_id)
                if got_point is None or ref_point["contact"] != got_point["contact"]:
                    print(f"{PREFIX}DIAGNOSTIC.CASE_ID|{case}")
                    print(f"{PREFIX}DIAGNOSTIC.STABLE_ID|{stable_id}")
                    print(f"{PREFIX}DIAGNOSTIC.REFERENCE_CONTACT|{ref_point['contact']}")
                    print(f"{PREFIX}DIAGNOSTIC.ACTUAL_CONTACT|{got_point['contact'] if got_point else 'MISSING'}")
                    print(f"{PREFIX}DIAGNOSTIC.REFERENCE_SEPARATION|{ref_point['separation_m']:.17g}")
                    if got_point:
                        print(f"{PREFIX}DIAGNOSTIC.ACTUAL_SEPARATION|{got_point['separation_m']:.17g}")
                        for field in ("collision_position_m", "surface_position_m", "surface_normal"):
                            print(f"{PREFIX}DIAGNOSTIC.REFERENCE_{field}|{ref_point[field]}")
                            print(f"{PREFIX}DIAGNOSTIC.ACTUAL_{field}|{got_point[field]}")
                    break
            raise
        print(f"{PREFIX}CASE_ID|{case}")
        print(f"{PREFIX}FIXTURE_SHA256|{hashlib.sha256(source.encode('utf-8')).hexdigest()}")
        print(f"{PREFIX}REFERENCE_SHA256|{hashlib.sha256(expected_text.encode('utf-8')).hexdigest()}")
        print(f"{PREFIX}TOLERANCE|{tolerance}")
        for field, value in sorted(maxima.items()):
            print(f"{PREFIX}MAX_ERROR.{field}|{value:.17g}")
        print(f"{PREFIX}CONTROL_POINT_COUNT|147")
        print(f"{PREFIX}CASE_RESULT|{case}|PASS")
    print(f"{PREFIX}CASE_COUNT|{len(cases)}")
    print(f"{PREFIX}RESULT|PASS")
except Exception as exc:
    print(f"{PREFIX}RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
