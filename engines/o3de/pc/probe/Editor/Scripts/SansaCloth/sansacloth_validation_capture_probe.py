"""BF-008: validation-only SR-001 C=0/G=0 JSONL capture from O3DE C++.

The C++ Gem supplies the 147 Final CP + Aggregate measurement records.
Python adds event metadata, writes a temporary artifact, and validates
the file read-back. This is not a production serialization contract.
"""

from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import tempfile
import uuid

PREFIX = "SANSA_O3DE|"
TEST_ID = "O3DE-SR-001-C0-G0"
BACKEND = "O3DE PC"
CP_COUNT = 147
TOLERANCE = 1.0e-5


def check(condition, message):
    if not condition:
        raise ValueError(message)


def close(actual, expected, label):
    check(isinstance(actual, (int, float)) and not isinstance(actual, bool),
          f"{label}: nonnumeric value")
    check(math.isfinite(actual) and abs(actual - expected) <= TOLERANCE,
          f"{label}: expected {expected}, got {actual}")


def check_vector(actual, expected, label):
    check(isinstance(actual, list) and len(actual) == 3,
          f"{label}: expected three-component vector")
    for axis, (a, e) in enumerate(zip(actual, expected)):
        close(a, e, f"{label}[{axis}]")


def validate_capture(records, event_id, timestamp_utc):
    check(len(records) == 149, f"record count: {len(records)}")
    header = records[0]
    check(header.get("record_type") == "header", "missing header")
    check(header.get("event_id") == event_id, "EventId mismatch")
    check(header.get("timestamp_utc") == timestamp_utc, "TimestampUtc mismatch")
    check(header.get("test_id") == TEST_ID, "TestId mismatch")
    check(header.get("backend") == BACKEND, "Backend mismatch")
    check(header.get("cp_count") == CP_COUNT, "header CPCount mismatch")
    print(f"{PREFIX}BF-008.VCF-001.RESULT|PASS")

    cp_records = records[1:-1]
    check(len(cp_records) == CP_COUNT, "Final CP record count mismatch")
    check(all(r.get("record_type") == "final_cp" for r in cp_records),
          "non-final_cp record in CP section")
    print(f"{PREFIX}BF-008.VCF-002.RESULT|PASS")

    ids = [r.get("stable_id") for r in cp_records]
    check(ids == list(range(CP_COUNT)), "Stable CP IDs are not contiguous 0..146")
    print(f"{PREFIX}BF-008.VCF-003.RESULT|PASS")

    support_count = 0
    contact_count = 0
    separation_sum = 0.0
    max_separation = 0.0
    max_penetration = 0.0
    max_deviation = 0.0
    deviation_square_sum = 0.0

    for r in cp_records:
        stable_id = r["stable_id"]
        u_index = stable_id % 21
        v_index = stable_id // 21
        u = u_index / 20.0
        v = v_index / 6.0
        ref = r.get("surface_reference")
        check(isinstance(ref, dict), f"CP {stable_id}: SurfaceReference absent")
        check(ref.get("domain_id") == 1, f"CP {stable_id}: DomainId")
        close(ref.get("u"), u, f"CP {stable_id}: U")
        close(ref.get("v"), v, f"CP {stable_id}: V")

        # File records are in canonical coordinates, not O3DE's Z-up basis.
        expected_position = [(u - 0.5) * 0.20, 0.0, (v - 0.5) * 0.10]
        check_vector(r.get("position_m"), expected_position,
                     f"CP {stable_id}: Final Position")
        check_vector(r.get("surface_normal"), [0.0, 1.0, 0.0],
                     f"CP {stable_id}: Surface Normal")
        separation = r.get("separation_m")
        close(separation, 0.0, f"CP {stable_id}: Separation")
        expected_anchor = u_index in (0, 20)
        expected_support = "Anchor" if expected_anchor else "Unsupported"
        check(r.get("support") == expected_support,
              f"CP {stable_id}: Support")
        expected_contact = separation <= 0.0
        check(r.get("derived_contact") is expected_contact,
              f"CP {stable_id}: Derived Contact")

        support_count += int(expected_anchor)
        contact_count += int(expected_contact)
        separation_sum += separation
        max_separation = max(max_separation, separation)
        max_penetration = max(max_penetration, max(0.0, -separation))
        deviation = math.dist(r["position_m"], expected_position)
        max_deviation = max(max_deviation, deviation)
        deviation_square_sum += deviation * deviation

    print(f"{PREFIX}BF-008.VCF-004.RESULT|PASS")
    print(f"{PREFIX}BF-008.VCF-005.RESULT|PASS")

    aggregate = records[-1]
    check(aggregate.get("record_type") == "aggregate", "missing aggregate")
    check(aggregate.get("contact_count") == contact_count == 147,
          "aggregate ContactCount mismatch")
    check(aggregate.get("support_count") == support_count == 14,
          "aggregate SupportCount mismatch")
    close(aggregate.get("mean_separation_m"), separation_sum / CP_COUNT,
          "MeanSeparation")
    close(aggregate.get("max_separation_m"), max_separation,
          "MaxSeparation")
    close(aggregate.get("max_penetration_m"), max_penetration,
          "MaxPenetration")
    close(aggregate.get("max_position_deviation_m"), max_deviation,
          "MaxPositionDeviation")
    close(aggregate.get("rms_position_deviation_m"),
          math.sqrt(deviation_square_sum / CP_COUNT), "RMSPositionDeviation")
    print(f"{PREFIX}BF-008.VCF-006.RESULT|PASS")
    return contact_count, support_count, aggregate


def run():
    try:
        import azlmbr.sansacloth_probe as probe
    except ImportError:
        print(f"{PREFIX}BF-008.DISCOVERY|CPP_GEM_NOT_LOADED")
        print(f"{PREFIX}BF-008.RESULT|OPEN")
        return False

    method = getattr(probe, "RunBf008Capture", None)
    if method is None:
        print(f"{PREFIX}BF-008.DISCOVERY|CPP_CAPTURE_METHOD_NOT_AVAILABLE")
        print(f"{PREFIX}BF-008.RESULT|OPEN")
        return False
    print(f"{PREFIX}BF-008.DISCOVERY|CPP_CAPTURE_AVAILABLE")

    # Never reconstruct CP results in Python: the Gem is the source.
    cpp_measurements = method()
    check(isinstance(cpp_measurements, str) and cpp_measurements.strip(),
          "C++ returned no JSONL measurements")
    print(f"{PREFIX}BF-008.PYTHON_BRIDGE_RESULT|PASS")

    event_id = str(uuid.uuid4())
    timestamp_utc = datetime.now(timezone.utc).isoformat(
        timespec="microseconds").replace("+00:00", "Z")
    header = {
        "record_type": "header",
        "event_id": event_id,
        "timestamp_utc": timestamp_utc,
        "test_id": TEST_ID,
        "backend": BACKEND,
        "cp_count": CP_COUNT,
    }
    lines = [json.dumps(header, separators=(",", ":"))]
    lines.extend(cpp_measurements.splitlines())
    check(len(lines) == 149, f"expected 149 lines, got {len(lines)}")

    path = Path(tempfile.gettempdir()) / (
        f"SansaClothO3DEValidationCapture-{event_id}.jsonl")
    pending_path = path.with_suffix(".jsonl.tmp")
    try:
        with pending_path.open("w", encoding="utf-8", newline="\n") as writer:
            writer.write("\n".join(lines) + "\n")
        os.replace(str(pending_path), str(path))
    finally:
        if pending_path.exists():
            pending_path.unlink()

    # Read the actual artifact, not the in-memory measurement buffer.
    with path.open("r", encoding="utf-8") as reader:
        records = [json.loads(line) for line in reader if line.strip()]
    contact_count, support_count, aggregate = validate_capture(
        records, event_id, timestamp_utc)
    print(f"{PREFIX}BF-008.VCF-007.RESULT|PASS")

    print(f"{PREFIX}BF-008.EVENT_ID|{event_id}")
    print(f"{PREFIX}BF-008.TIMESTAMP_UTC|{timestamp_utc}")
    print(f"{PREFIX}BF-008.TEST_ID|{TEST_ID}")
    print(f"{PREFIX}BF-008.BACKEND|{BACKEND}")
    print(f"{PREFIX}BF-008.CAPTURE_PATH|{path}")
    print(f"{PREFIX}BF-008.RECORD_COUNT|{len(records)}")
    print(f"{PREFIX}BF-008.CP_COUNT|{CP_COUNT}")
    print(f"{PREFIX}BF-008.CONTACT_COUNT|{contact_count}")
    print(f"{PREFIX}BF-008.SUPPORT_COUNT|{support_count}")
    for field in ("mean_separation_m", "max_separation_m", "max_penetration_m",
                  "max_position_deviation_m", "rms_position_deviation_m"):
        print(f"{PREFIX}BF-008.{field.upper()}|{aggregate[field]}")
    print(f"{PREFIX}BF-008.RESULT|PASS")
    return True


try:
    result = run()
except Exception as exc:
    print(f"{PREFIX}BF-008.RESULT|FAIL")
    print(f"{PREFIX}BF-008.PYTHON_PROBE_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
else:
    print(f"{PREFIX}BF-008.PYTHON_PROBE_RESULT|{'PASS' if result else 'OPEN'}")
