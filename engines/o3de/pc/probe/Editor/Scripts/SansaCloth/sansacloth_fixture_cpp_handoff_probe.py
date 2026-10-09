"""OXC-002: pass a validated, committed Reference JSON to C++ Gem."""
import hashlib
import gc
import importlib.util
import json
from pathlib import Path

PREFIX = "SANSA_O3DE|"
try:
    import azlmbr.sansacloth_probe as probe
    fn = getattr(probe, "ProbeFixtureJson")
except (ImportError, AttributeError):
    print(f"{PREFIX}OXC-002.DISCOVERY|CPP_GEM_NOT_LOADED")
    print(f"{PREFIX}OXC-002.RESULT|OPEN")
    raise

try:
    root = Path(__file__).resolve().parent
    importer_path = root / "sansacloth_fixture_exchange_import_probe.py"
    spec = importlib.util.spec_from_file_location("sansacloth_fixture_import", importer_path)
    importer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(importer)  # also runs existing OXF regression
    path = importer.fixture_path()
    source = path.read_text(encoding="utf-8")
    document = importer.parse_json(source)
    mapped = importer.import_fixture(document)
    importer.check_import(mapped)
    digest = hashlib.sha256(source.encode("utf-8")).hexdigest()
    ack = fn(source)
    expected = "OXC-002|ACK|147|240|147|14|147"
    if ack != expected:
        raise ValueError(f"C++ fixture ACK mismatch: {ack!r}")
    print(f"{PREFIX}OXC-002.SOURCE_SHA256|{digest}")
    print(f"{PREFIX}OXC-002.REAL_JSON_ROUNDTRIP|PASS")
    if fn('{"format":"invalid"}') != "":
        raise ValueError("C++ accepted invalid fixture JSON")
    print(f"{PREFIX}OXC-002.INVALID_REJECTED|PASS")
    # OXC-005 negative cases cross the C++ boundary directly; the Python
    # importer is deliberately not called for these mutated payloads.
    for label, point_id, field, replacement in (
        ("ANCHOR_PATTERN", 1, "anchor", True),
        ("CONTACT_MISSING", 1, "contact", False),
    ):
        invalid = json.loads(source)
        invalid["cloth"]["control_points"][point_id][field] = replacement
        if fn(json.dumps(invalid, ensure_ascii=False)) != "":
            raise ValueError(f"C++ accepted invalid OXC-005 {label}")
        print(f"{PREFIX}OXC-005.{label}|REJECTED")
    print(f"{PREFIX}OXC-005.NEGATIVE_RESULT|PASS")
    # OXC-006: send corrupted input parameters directly to C++.
    for label, field, replacement in (
        ("GRAVITY_NONZERO", "world_gravity_m_per_s2", [0.0, -9.81, 0.0]),
        ("GRAVITY_LENGTH", "world_gravity_m_per_s2", [0.0, 0.0]),
        ("GRAVITY_TYPE", "world_gravity_m_per_s2", [0.0, "bad", 0.0]),
        ("CONFORMITY_NONZERO", "conformity", 0.1),
        ("CONFORMITY_TYPE", "conformity", "bad"),
        ("TOLERANCE_NONZERO", "collision_tolerance_m", 0.001),
        ("TOLERANCE_NEGATIVE", "collision_tolerance_m", -0.001),
        ("TOLERANCE_TYPE", "collision_tolerance_m", "bad"),
        ("GRAVITY_NONFINITE", "world_gravity_m_per_s2", [0.0, 1e309, 0.0]),
    ):
        invalid = json.loads(source)
        invalid["inputs"][field] = replacement
        if fn(json.dumps(invalid, ensure_ascii=False)) != "":
            raise ValueError(f"C++ accepted invalid OXC-006 {label}")
        print(f"{PREFIX}OXC-006.{label}|REJECTED")
    for field in ("world_gravity_m_per_s2", "conformity", "collision_tolerance_m"):
        invalid = json.loads(source)
        del invalid["inputs"][field]
        if fn(json.dumps(invalid, ensure_ascii=False)) != "":
            raise ValueError(f"C++ accepted missing OXC-006 {field}")
        print(f"{PREFIX}OXC-006.MISSING_{field.upper()}|REJECTED")
    print(f"{PREFIX}OXC-006.NEGATIVE_RESULT|PASS")


    # OXC-007: exercise the Python/C++ ownership boundary repeatedly.
    # The C++ method parses into a call-local RapidJSON Document and returns
    # an owned AZStd::string; Python must not depend on prior input buffers.
    # Interleave valid/invalid calls and release temporary Python objects.
    invalid_payload = json.loads(source)
    invalid_payload["inputs"]["conformity"] = 0.25
    invalid_text = json.dumps(invalid_payload, ensure_ascii=False)
    del invalid_payload
    for iteration in range(32):
        # New temporary strings force separate Python-side allocations.
        valid_text = source.encode("utf-8").decode("utf-8")
        result = fn(valid_text)
        del valid_text
        if result != expected:
            raise ValueError(f"OXC-007 valid iteration {iteration} ACK mismatch: {result!r}")
        if fn(invalid_text) != "":
            raise ValueError(f"OXC-007 invalid iteration {iteration} was accepted")
        del result
        if iteration % 8 == 7:
            gc.collect()
    print(f"{PREFIX}OXC-007.INTERLEAVED_32|PASS")

    # A previously rejected temporary input must not affect subsequent calls.
    for iteration in range(8):
        transient = json.loads(source)
        transient["inputs"]["collision_tolerance_m"] = 0.001
        transient_text = json.dumps(transient, ensure_ascii=False)
        if fn(transient_text) != "":
            raise ValueError(f"OXC-007 transient invalid iteration {iteration} accepted")
        del transient, transient_text
        gc.collect()
        if fn(source) != expected:
            raise ValueError(f"OXC-007 valid recovery iteration {iteration} failed")
    print(f"{PREFIX}OXC-007.RECOVERY_8|PASS")
    print(f"{PREFIX}OXC-007.RESULT|PASS")

    print(f"{PREFIX}OXC-002.RESULT|PASS")
except Exception as exc:
    print(f"{PREFIX}OXC-002.RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
