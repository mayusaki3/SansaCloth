"""OXC-002: pass a validated, committed Reference JSON to C++ Gem."""
import hashlib
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

    print(f"{PREFIX}OXC-002.RESULT|PASS")
except Exception as exc:
    print(f"{PREFIX}OXC-002.RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
