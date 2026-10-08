"""OXF-011..019: validation-only 30-case Reference Fixture Exchange matrix import."""
import hashlib
import importlib.util
from pathlib import Path

PREFIX = "SANSA_O3DE|"
HERE = Path(__file__).resolve().parent
EXPECTED = {f"SR-{s:03d}-C{c}-G{g}" for s in range(1, 6)
            for c in ("0", "05", "1") for g in (0, 1)}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def load_importer():
    source = HERE / "sansacloth_fixture_exchange_import_probe.py"
    require(source.is_file(), f"missing importer: {source}")
    # The legacy importer executes its own SR-001 regression on import.
    spec = importlib.util.spec_from_file_location("sansacloth_oxf_single", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run():
    folder = HERE / "Fixtures"
    require(folder.is_dir(), f"missing fixture directory: {folder}")
    files = {p.stem: p for p in folder.glob("*.json")}
    require(set(files) == EXPECTED,
            f"matrix missing={sorted(EXPECTED-set(files))} extra={sorted(set(files)-EXPECTED)}")
    print(f"{PREFIX}OXF-011.RESULT|PASS")
    importer = load_importer()
    print(f"{PREFIX}OXF-012.RESULT|PASS")
    mapped_by_scenario = {}
    source_by_scenario = {}
    for case_id in sorted(EXPECTED):
        raw = files[case_id].read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        doc = importer.parse_json(raw.decode("utf-8"))
        require(doc["case_id"] == case_id, f"{case_id}: case_id mismatch")
        mapped = importer.import_fixture(doc, expected_case_id=case_id)
        require(mapped["vertices"] and mapped["triangles"] and mapped["cp"],
                f"{case_id}: empty geometry")
        for tri in mapped["triangles"]:
            a, b, c = (mapped["vertices"][i] for i in tri)
            n = importer.cross(importer.subtract(b, a), importer.subtract(c, a))
            require(importer.dot(n, n) > 1e-16, f"{case_id}: degenerate triangle")
        scenario = case_id.split("-C")[0]
        stable = {"vertices": mapped["vertices"], "normals": mapped["normals"],
                  "uvs": mapped["uvs"], "triangles": mapped["triangles"],
                  "cp": mapped["cp"]}
        if scenario in mapped_by_scenario:
            require(mapped_by_scenario[scenario] == stable,
                    f"{case_id}: scenario geometry/semantics changed")
        else:
            mapped_by_scenario[scenario] = stable
            source_by_scenario[scenario] = doc
        require(mapped["collision"] == 0, f"{case_id}: collision tolerance")
        c_code, g_code = case_id.split("-C")[1].split("-G")
        require(abs(mapped["conformity"] - {"0": 0, "05": 0.5, "1": 1}[c_code]) < 1e-9,
                f"{case_id}: conformity mismatch")
        require((importer.dot(mapped["gravity"], mapped["gravity"]) > 0) == (g_code == "1"),
                f"{case_id}: gravity mismatch")
        require(len(mapped["cp"]) == len(set(mapped["cp"])), f"{case_id}: duplicate CP")
        if scenario == "SR-003":
            require(sum(v["anchor"] for v in mapped["cp"].values()) == 7,
                    f"{case_id}: SR-003 anchor count")
        print(f"{PREFIX}OXF.MATRIX_CASE|{case_id}|{digest}|PASS")
    print(f"{PREFIX}OXF-013.RESULT|PASS")
    print(f"{PREFIX}OXF-014.RESULT|PASS")
    print(f"{PREFIX}OXF-015.RESULT|PASS")
    print(f"{PREFIX}OXF-016.RESULT|PASS")
    print(f"{PREFIX}OXF-017.RESULT|PASS")
    print(f"{PREFIX}OXF-018.RESULT|PASS")
    print(f"{PREFIX}OXF-019.CASE_COUNT|{len(EXPECTED)}")
    print(f"{PREFIX}OXF-019.SCENARIO_COUNT|{len(mapped_by_scenario)}")
    require(len(mapped_by_scenario) == 5, "scenario count")
    print(f"{PREFIX}OXF-019.RESULT|PASS")
    print(f"{PREFIX}OXF.MATRIX_RESULT|PASS")


try:
    run()
except Exception as exc:
    print(f"{PREFIX}OXF.MATRIX_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
