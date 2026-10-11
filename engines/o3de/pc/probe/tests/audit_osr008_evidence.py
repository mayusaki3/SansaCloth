"""Audit OSR-008 against Git originals at the captured revision.

Optional --regenerate re-runs Rust Reference into a temporary directory, without
modifying tracked validation artifacts. Python 3.10+; no O3DE Editor required.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[5]
MANIFEST = ROOT / "validation/osr008-evidence/osr008-evidence.json"
HEX = re.compile(r"^[a-f0-9]{64}$")
FIELDS = ("bridge_position_m", "gravity_position_m", "conformity_position_m",
          "collision_position_m", "final_position_m", "surface_position_m",
          "surface_normal", "separation_m")

def git_blob(revision, path):
    return subprocess.run(["git", "-C", str(ROOT), "show", f"{revision}:{path}"],
                          capture_output=True, check=True).stdout

def check_hash(label, raw, expected, errors):
    actual = hashlib.sha256(raw).hexdigest()
    if not isinstance(expected, str) or not HEX.fullmatch(expected) or actual != expected:
        errors.append(f"{label}: SHA-256 mismatch expected={expected} actual={actual}")

def audit(regenerate=False):
    errors = []
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    revision = manifest["repository_revision"]
    expected_ids = {f"SR-{s:03d}-C{c}-G{g}"
                    for s in range(1, 6) for c in ("0", "05", "1") for g in ("0", "1")}
    records = manifest["cases"]
    ids = [r["case_id"] for r in records]
    if len(ids) != 30 or len(set(ids)) != 30 or set(ids) != expected_ids:
        errors.append("case IDs missing, duplicated or unexpected")
    counts = {k: sum(r["status"] == k for r in records) for k in ("PASS", "FAIL", "OPEN")}
    if counts != {"PASS": 30, "FAIL": 0, "OPEN": 0} or counts != manifest["counts"]:
        errors.append(f"counts inconsistent: {counts}")
    if manifest.get("result") != "PASS" or manifest.get("tracked_worktree_status") != "CLEAN":
        errors.append("manifest not marked PASS/CLEAN")
    if manifest.get("reference_version") != "sansacloth.validation.surface-response-result/0":
        errors.append("reference version mismatch")
    if manifest.get("fixture_version") != "sansacloth.validation.fixture-exchange/0":
        errors.append("fixture version mismatch")
    for label, item in (("backend_source", manifest["backend_source"]),
                        ("comparator", manifest["comparator"])):
        path = item["path"].replace("\\", "/")
        try:
            raw = git_blob(revision, path)
            check_hash(label, raw, item["sha256"], errors)
            if len(raw) != item["size_bytes"]:
                errors.append(f"{label}: size mismatch")
        except subprocess.CalledProcessError:
            errors.append(f"{label}: cannot read original at {revision}")
    for record in records:
        case = record["case_id"]
        if record["status"] != "PASS" or record["error"] is not None:
            errors.append(f"{case}: not a clean PASS")
        values = record.get("max_abs_error") or {}
        if set(values) != set(FIELDS) or any(v != 0 for v in values.values()):
            errors.append(f"{case}: numeric errors not all zero")
        if not HEX.fullmatch(record.get("backend_output_sha256", "")):
            errors.append(f"{case}: invalid backend output hash")
        for kind, expected_format in (
            ("fixture", "sansacloth.validation.fixture-exchange/0"),
            ("reference", "sansacloth.validation.surface-response-result/0"),
        ):
            item = record[kind]
            path = item["path"].replace("\\", "/")
            if not path.endswith("/" + case + ".json"):
                errors.append(f"{case}: unexpected {kind} path")
            try:
                raw = git_blob(revision, path)
                check_hash(f"{case} {kind}", raw, item["sha256"], errors)
                if len(raw) != item["size_bytes"]:
                    errors.append(f"{case} {kind}: size mismatch")
                parsed = json.loads(raw)
                if parsed.get("format") != expected_format or parsed.get("case_id") != case:
                    errors.append(f"{case} {kind}: format/case mismatch")
            except (subprocess.CalledProcessError, ValueError) as exc:
                errors.append(f"{case} {kind}: original unavailable or invalid ({exc})")
    print(f"OSR008_AUDIT|ORIGINAL_HASHES|{'PASS' if not errors else 'FAIL'}")
    if regenerate:
        with tempfile.TemporaryDirectory(prefix="sansacloth-osr001-") as temp:
            base = Path(temp)
            canonical = base / "canonical"
            roundtrip = base / "roundtrip"
            env = dict(os.environ, SANSA_SURFACE_JSON_ROUNDTRIP_RESULT_DIR=str(roundtrip))
            proc = subprocess.run(
                ["cargo", "run", "-p", "sansacloth-reference",
                 "--example", "export_surface_response_result_matrix", "--", str(canonical)],
                cwd=ROOT / "reference", env=env, capture_output=True, text=True,
            )
            if proc.returncode:
                errors.append("Rust regeneration failed: " + proc.stderr[-1600:])
            else:
                for record in records:
                    case = record["case_id"]
                    result_path = roundtrip / (case + ".json")
                    if not result_path.is_file():
                        errors.append(f"{case}: regenerated result missing")
                        continue
                    try:
                        expected = json.loads(git_blob(revision, record["reference"]["path"].replace("\\", "/")))
                        actual = json.loads(result_path.read_text(encoding="utf-8"))
                        if expected != actual:
                            errors.append(f"{case}: regenerated JSON differs from recorded Reference")
                    except (subprocess.CalledProcessError, ValueError) as exc:
                        errors.append(f"{case}: regeneration comparison failed: {exc}")
        print(f"OSR008_AUDIT|REFERENCE_REGENERATION|{'PASS' if not errors else 'FAIL'}")
    for issue in errors:
        print("OSR008_AUDIT|ERROR|" + issue)
    print(f"OSR008_AUDIT|ERROR_COUNT|{len(errors)}")
    print(f"OSR008_AUDIT|RESULT|{'PASS' if not errors else 'FAIL'}")
    return not errors

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--regenerate", action="store_true")
    args = parser.parse_args()
    sys.exit(0 if audit(args.regenerate) else 1)
