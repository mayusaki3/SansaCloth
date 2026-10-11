"""OSR-006: execute independent O3DE solver and compare all 30 SurfaceResponse results.

The thresholds below are diagnostic defaults, NOT an OSR-006 acceptance decision.
Requires O3DE Editor Python with azlmbr.sansacloth_probe.
"""
import importlib.util
import json
from pathlib import Path

import azlmbr.sansacloth_probe as probe

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parents[6]
fixture_dir = repo_root / "reference/validation/fixture-exchange/basic-v1"
result_dir = repo_root / "validation/surface-response-json-roundtrip"
compare_path = script_dir / "sansacloth_surface_response_compare.py"
spec = importlib.util.spec_from_file_location("sansacloth_osr_compare", compare_path)
compare_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare_module)

# Provisional numeric comparison only; review documented OSR-006 limits
# before converting the diagnostic result into a formal PASS.
POSITION_TOLERANCE_M = 1e-12
NORMAL_TOLERANCE = 1e-12

checked = 0
failed = 0
maxima = {field: 0.0 for field in compare_module.VECTORS + compare_module.SCALARS}
for scenario in range(1, 6):
    for conformity in ("0", "05", "1"):
        for gravity in ("0", "1"):
            case = f"SR-{scenario:03d}-C{conformity}-G{gravity}"
            try:
                fixture = (fixture_dir / f"{case}.json").read_text(encoding="utf-8")
                reference = json.loads((result_dir / f"{case}.json").read_text(encoding="utf-8"))
                response = probe.ProbeSurfaceResponseJson(fixture)
                if not response:
                    raise ValueError("O3DE returned no solver output")
                actual = json.loads(response)
                differences = compare_module.compare(
                    reference, actual,
                    absolute_tolerance_m=POSITION_TOLERANCE_M,
                    normal_tolerance=NORMAL_TOLERANCE,
                )
                for field, value in differences.items():
                    maxima[field] = max(maxima[field], value)
                print(f"SANSA_O3DE|OSR-006.FULL_CASE|{case}|PASS")
            except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
                failed += 1
                print(f"SANSA_O3DE|OSR-006.FULL_CASE|{case}|FAIL|{error}")
            checked += 1

for field, value in maxima.items():
    print(f"SANSA_O3DE|OSR-006.MAX_DELTA|{field}|{value:.17g}")
print(f"SANSA_O3DE|OSR-006.FULL_CHECKED|{checked}")
print(f"SANSA_O3DE|OSR-006.FULL_FAILED|{failed}")
print(f"SANSA_O3DE|OSR-006.FULL_RESULT|{'PASS_DIAGNOSTIC' if checked == 30 and failed == 0 else 'FAIL'}")
print("SANSA_O3DE|OSR-006.FORMAL_RESULT|PENDING_CRITERIA_REVIEW")
