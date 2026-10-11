"""OSR-007 runtime negative probe: real C++ solver output plus deliberate mutations."""
import copy
import importlib.util
import json
from pathlib import Path

import azlmbr.sansacloth_probe as probe

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parents[6]
case = "SR-002-C05-G1"
fixture_path = repo_root / "reference/validation/fixture-exchange/basic-v1" / (case + ".json")
reference_path = repo_root / "validation/surface-response-json-roundtrip" / (case + ".json")
spec = importlib.util.spec_from_file_location("osr007_compare", script_dir / "sansacloth_surface_response_compare.py")
comparator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparator)

prefix = "SANSA_O3DE|OSR-007."
expected = json.loads(reference_path.read_text(encoding="utf-8"))
fixture = fixture_path.read_text(encoding="utf-8")
output = probe.ProbeSurfaceResponseJson(fixture)
if not output:
    raise RuntimeError("OSR-007: C++ solver returned empty output")
actual = json.loads(output)
comparator.compare(expected, actual, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)
print(prefix + "BASELINE|PASS")

tests = {}
for field in comparator.VECTORS + comparator.SCALARS:
    changed = copy.deepcopy(actual)
    if field in comparator.VECTORS:
        changed["control_points"][42][field][1] += 0.001
    else:
        changed["control_points"][42][field] += 0.001
    tests["NUMERIC_" + field] = changed

changed = copy.deepcopy(actual)
changed["control_points"][42]["contact"] = not changed["control_points"][42]["contact"]
changed["aggregate"]["contact_count"] += 1 if changed["control_points"][42]["contact"] else -1
tests["CONTACT"] = changed
changed = copy.deepcopy(actual)
changed["control_points"][42]["support"] = "Anchor" if changed["control_points"][42]["support"] == "Unsupported" else "Unsupported"
changed["aggregate"]["support_count"] += 1 if changed["control_points"][42]["support"] == "Anchor" else -1
tests["SUPPORT"] = changed
changed = copy.deepcopy(actual)
changed["profile"]["support_layout"] = "OSR007-invalid"
tests["PROFILE"] = changed
tests["ACK_ONLY"] = {"ack": "accepted", "case_id": case}

passed = 0
for name, changed in tests.items():
    try:
        comparator.compare(expected, changed, absolute_tolerance_m=1e-12, normal_tolerance=1e-12)
    except (ValueError, KeyError, TypeError) as error:
        passed += 1
        print(f"{prefix}NEGATIVE|{name}|REJECTED|{error}")
    else:
        print(f"{prefix}NEGATIVE|{name}|UNEXPECTED_ACCEPT")
print(f"{prefix}NEGATIVE_COUNT|{len(tests)}")
print(f"{prefix}REJECTED_COUNT|{passed}")
print(f"{prefix}RESULT|{'PASS' if passed == len(tests) else 'FAIL'}")
