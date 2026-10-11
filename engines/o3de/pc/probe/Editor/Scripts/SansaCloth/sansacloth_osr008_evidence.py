"""OSR-008: independent O3DE runtime evidence for the complete 30-case matrix.

Requires O3DE Editor Python, repository fixture/reference artifacts and Gem source.
Does not mark missing or failed cases PASS. Writes an auditable JSON manifest.
"""
import hashlib
import importlib.util
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

import azlmbr.sansacloth_probe as probe

script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parents[6]
fixture_dir = repo_root / "reference/validation/fixture-exchange/basic-v1"
reference_dir = repo_root / "validation/surface-response-json-roundtrip"
backend_source = repo_root / "engines/o3de/pc/probe/Gem/SansaClothBackendProbeValidation/Source/SansaClothBackendProbeValidationSystemComponent.cpp"
comparator_path = script_dir / "sansacloth_surface_response_compare.py"
spec = importlib.util.spec_from_file_location("osr008_comparator", comparator_path)
comparator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(comparator)

POSITION_TOLERANCE_M = 1e-12  # diagnostic comparison threshold, not a normative product tolerance
NORMAL_TOLERANCE = 1e-12
prefix = "SANSA_O3DE|OSR-008."

def sha256(data):
    return hashlib.sha256(data).hexdigest()

def version():
    try:
        proc = subprocess.run(
            ["git", "-C", str(repo_root), "rev-parse", "HEAD"],
            capture_output=True, text=True, timeout=10, check=True,
        )
        return proc.stdout.strip() or "UNKNOWN"
    except (OSError, subprocess.SubprocessError):
        return "UNKNOWN"

def file_evidence(path):
    raw = path.read_bytes()
    return {"path": str(path.relative_to(repo_root)), "sha256": sha256(raw), "size_bytes": len(raw)}

revision = version()
backend = file_evidence(backend_source)
comparator_info = file_evidence(comparator_path)
records = []
for scenario in range(1, 6):
    for conformity in ("0", "05", "1"):
        for gravity in ("0", "1"):
            case = f"SR-{scenario:03d}-C{conformity}-G{gravity}"
            record = {"case_id": case, "status": "OPEN", "max_abs_error": None, "error": None}
            try:
                fixture_path = fixture_dir / (case + ".json")
                reference_path = reference_dir / (case + ".json")
                record["fixture"] = file_evidence(fixture_path)
                record["reference"] = file_evidence(reference_path)
                fixture_text = fixture_path.read_text(encoding="utf-8")
                reference = json.loads(reference_path.read_text(encoding="utf-8"))
                result_text = probe.ProbeSurfaceResponseJson(fixture_text)
                if not result_text:
                    raise ValueError("C++ solver returned no output")
                record["backend_output_sha256"] = sha256(result_text.encode("utf-8"))
                actual = json.loads(result_text)
                maxima = comparator.compare(
                    reference, actual,
                    absolute_tolerance_m=POSITION_TOLERANCE_M,
                    normal_tolerance=NORMAL_TOLERANCE,
                )
                record["max_abs_error"] = maxima
                record["status"] = "PASS"
            except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as error:
                record["status"] = "FAIL" if record.get("backend_output_sha256") else "OPEN"
                record["error"] = str(error)
            records.append(record)
            print(f"{prefix}CASE|{case}|{record['status']}")

counts = {state: sum(r["status"] == state for r in records) for state in ("PASS", "FAIL", "OPEN")}
manifest = {
    "gate": "OSR-008",
    "scope": "O3DE validation-only analytic fixture C++ solver versus Rust Reference",
    "generated_at_utc": datetime.now(timezone.utc).isoformat(),
    "repository_revision": revision,
    "reference_version": "sansacloth.validation.surface-response-result/0",
    "fixture_version": "sansacloth.validation.fixture-exchange/0",
    "backend_api": "azlmbr.sansacloth_probe.ProbeSurfaceResponseJson",
    "backend_source": backend,
    "comparator": comparator_info,
    "position_tolerance_m": POSITION_TOLERANCE_M,
    "normal_tolerance": NORMAL_TOLERANCE,
    "counts": counts,
    "cases": records,
}
# Require a reproducible source revision; a dirty tree must not be silently accepted.
try:
    dirty = subprocess.run(
        ["git", "-C", str(repo_root), "status", "--porcelain", "--untracked-files=no"],
        capture_output=True, text=True, timeout=10, check=True,
    ).stdout.strip()
except (OSError, subprocess.SubprocessError):
    dirty = "UNKNOWN"
manifest["tracked_worktree_status"] = "CLEAN" if not dirty else ("UNKNOWN" if dirty == "UNKNOWN" else "MODIFIED")
ready = (len(records) == 30 and counts == {"PASS": 30, "FAIL": 0, "OPEN": 0}
         and revision != "UNKNOWN" and not dirty)
manifest["result"] = "PASS" if ready else "OPEN"
output_dir = repo_root / "validation/osr008-evidence"
output_dir.mkdir(parents=True, exist_ok=True)
output_file = output_dir / "osr008-evidence.json"
output_file.write_text(json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print(f"{prefix}CASE_COUNT|{len(records)}")
for state in ("PASS", "FAIL", "OPEN"):
    print(f"{prefix}{state}_COUNT|{counts[state]}")
print(f"{prefix}REPOSITORY_REVISION|{revision}")
print(f"{prefix}TRACKED_WORKTREE|{manifest['tracked_worktree_status']}")
print(f"{prefix}BACKEND_SHA256|{backend['sha256']}")
print(f"{prefix}MANIFEST|{output_file}")
print(f"{prefix}RESULT|{manifest['result']}")
