"""OXF-001..010: import Rust SR-001-C0-G0 resolved Fixture Exchange in O3DE Editor.

Validation-only Python importer. Does not create a production O3DE mesh
or run SurfaceResponse. No analytic fixture is generated as input.
"""
import copy
import hashlib
import json
import math
from pathlib import Path

PREFIX = "SANSA_O3DE|"
FILENAME = "SR-001-C0-G0.json"
FORMAT = "sansacloth.validation.fixture-exchange/0"
TOL = 1e-6


class FixtureImportError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise FixtureImportError(message)


def obj(value, keys, name):
    require(type(value) is dict, f"{name}: expected object")
    require(set(value) == set(keys),
            f"{name}: keys {sorted(set(value) ^ set(keys))}")
    return value


def arr(value, length, name):
    require(type(value) is list and len(value) == length,
            f"{name}: expected {length} items")
    return value


def number(value, name):
    require(type(value) in (float, int) and math.isfinite(value),
            f"{name}: expected finite number")
    return float(value)


def uint(value, name):
    require(type(value) is int and 0 <= value <= 2**64 - 1,
            f"{name}: expected unsigned integer")
    return value


def vector(value, count, name):
    return tuple(number(v, f"{name}[{i}]")
                 for i, v in enumerate(arr(value, count, name)))


def uv(value, name):
    v = vector(value, 2, name)
    require(all(0 <= a <= 1 for a in v), f"{name}: UV outside [0,1]")
    return v


def near(a, b, name, tolerance=TOL):
    require(abs(a - b) <= tolerance, f"{name}: {a} != {b}")


def near_vector(a, b, name, tolerance=TOL):
    require(len(a) == len(b), f"{name}: vector length mismatch")
    for i, (x, y) in enumerate(zip(a, b)):
        near(x, y, f"{name}[{i}]", tolerance)


def canonical_to_o3de(v):
    return (v[0], v[2], v[1])


def subtract(a, b):
    return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1],
            a[2]*b[0]-a[0]*b[2],
            a[0]*b[1]-a[1]*b[0])


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def reject_constant(value):
    raise FixtureImportError(f"nonfinite JSON constant {value}")


def reject_duplicate_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key {key}")
        result[key] = value
    return result


def parse_json(text):
    try:
        return json.loads(text, parse_constant=reject_constant,
                          object_pairs_hook=reject_duplicate_pairs)
    except (ValueError, TypeError, RecursionError) as exc:
        raise FixtureImportError(f"invalid JSON: {exc}") from exc


def import_fixture(doc, expected_case_id="SR-001-C0-G0"):
    root = obj(doc, ("format", "case_id", "coordinate", "body_surface",
                     "cloth", "inputs"), "$")
    require(root["format"] == FORMAT, "unsupported format")
    require(root["case_id"] == expected_case_id, "unexpected case_id")
    coord = obj(root["coordinate"], ("length_unit", "gravity_unit", "x",
                                    "y", "z"), "coordinate")
    require(coord == {"length_unit": "m", "gravity_unit": "m/s^2",
                      "x": "right", "y": "up", "z": "forward"},
            "coordinate/unit declaration mismatch")

    body = obj(root["body_surface"],
               ("domain_id", "vertices", "triangles"), "body_surface")
    domain = uint(body["domain_id"], "domain_id")
    require(domain == 1, "expected DomainId=1")
    require(type(body["vertices"]) is list and body["vertices"],
            "empty body vertices")
    require(type(body["triangles"]) is list and body["triangles"],
            "empty body triangles")

    vertices = []
    normals = []
    uvs = []
    for i, entry in enumerate(body["vertices"]):
        entry = obj(entry, ("position_m", "normal", "uv"), f"vertex {i}")
        pos = vector(entry["position_m"], 3, f"vertex {i} position")
        normal = vector(entry["normal"], 3, f"vertex {i} normal")
        require(0 < dot(normal, normal) < float("inf"),
                f"vertex {i}: invalid normal")
        vertices.append(canonical_to_o3de(pos))
        normals.append(canonical_to_o3de(normal))
        uvs.append(uv(entry["uv"], f"vertex {i} UV"))

    triangles = []
    for i, entry in enumerate(body["triangles"]):
        tri = tuple(uint(v, f"triangle {i} index")
                    for v in arr(entry, 3, f"triangle {i}"))
        require(all(v < len(vertices) for v in tri),
                f"triangle {i}: index out of range")
        require(len(set(tri)) == 3, f"triangle {i}: repeated vertex")
        triangles.append((tri[0], tri[2], tri[1]))  # basis swap flips winding

    cloth = obj(root["cloth"], ("control_points",), "cloth")
    points = cloth["control_points"]
    require(type(points) is list and points, "empty cloth control_points")
    cp = {}
    strip_orders = set()
    for i, entry in enumerate(points):
        entry = obj(entry, ("stable_id", "strip_id", "strip_order",
                            "position_m", "surface_reference",
                            "anchor", "contact"), f"CP {i}")
        stable = uint(entry["stable_id"], f"CP {i} stable_id")
        strip = uint(entry["strip_id"], f"CP {i} strip_id")
        order = uint(entry["strip_order"], f"CP {i} strip_order")
        require(stable not in cp, f"duplicate stable_id {stable}")
        require((strip, order) not in strip_orders,
                f"duplicate strip/order {strip}/{order}")
        strip_orders.add((strip, order))
        ref = obj(entry["surface_reference"], ("domain_id", "u", "v"),
                  f"CP {i} SurfaceReference")
        require(uint(ref["domain_id"], f"CP {i} domain") == domain,
                f"CP {i}: unknown domain")
        uv_ref = uv([ref["u"], ref["v"]], f"CP {i} SurfaceReference UV")
        require(type(entry["anchor"]) is bool, f"CP {i}: anchor not bool")
        require(type(entry["contact"]) is bool, f"CP {i}: contact not bool")
        cp[stable] = {
            "position": canonical_to_o3de(
                vector(entry["position_m"], 3, f"CP {i} position")),
            "reference": (domain, uv_ref),
            "strip": (strip, order),
            "anchor": entry["anchor"],
            "contact": entry["contact"],
        }

    inputs = obj(root["inputs"], ("world_gravity_m_per_s2", "conformity",
                                 "collision_tolerance_m"), "inputs")
    gravity = canonical_to_o3de(
        vector(inputs["world_gravity_m_per_s2"], 3, "world gravity"))
    conformity = number(inputs["conformity"], "conformity")
    collision = number(inputs["collision_tolerance_m"], "collision tolerance")
    require(0 <= conformity <= 1, "conformity outside [0,1]")
    require(collision >= 0, "negative collision tolerance")
    return {"vertices": vertices, "normals": normals, "uvs": uvs,
            "triangles": triangles, "cp": cp, "gravity": gravity,
            "conformity": conformity, "collision": collision}


def check_import(mapped):
    vertices = mapped["vertices"]
    normals = mapped["normals"]
    triangles = mapped["triangles"]
    cp = mapped["cp"]

    require(len(vertices) == 147 and len(triangles) == 240,
            "body vertex/triangle count mismatch")
    near(max(v[0] for v in vertices)-min(v[0] for v in vertices),
         0.20, "body width")
    near(max(v[1] for v in vertices)-min(v[1] for v in vertices),
         0.10, "body depth")
    for i, (p, n) in enumerate(zip(vertices, normals)):
        near(p[2], 0, f"body height {i}")
        near_vector(n, (0, 0, 1), f"body normal {i}")
    print(f"{PREFIX}OXF-002.RESULT|PASS")

    for i, (a, b, c) in enumerate(triangles):
        normal = cross(subtract(vertices[b], vertices[a]),
                       subtract(vertices[c], vertices[a]))
        require(dot(normal, normal) > 1e-16,
                f"triangle {i}: degenerate")
        require(normal[2] > 0, f"triangle {i}: wrong winding")
        cell = i // 2
        row, col = divmod(cell, 20)
        v00 = row*21+col
        v01 = v00+21
        v11 = v01+1
        v10 = v00+1
        expected = ((v00, v11, v01), (v00, v10, v11))[i % 2]
        require((a, b, c) == expected, f"triangle {i}: topology mismatch")
    print(f"{PREFIX}OXF-003.RESULT|PASS")

    require(len(cp) == 147 and set(cp) == set(range(147)),
            "CP count/identity mismatch")
    anchor_count = 0
    contact_count = 0
    for stable_id in range(147):
        entry = cp[stable_id]
        row, col = divmod(stable_id, 21)
        expected_uv = (col/20, row/6)
        require(entry["reference"][0] == 1,
                f"CP {stable_id}: domain mismatch")
        near_vector(entry["reference"][1], expected_uv,
                    f"CP {stable_id} reference UV")
        near_vector(mapped["uvs"][stable_id], expected_uv,
                    f"body vertex {stable_id} UV")
        require(entry["strip"] == (row, col),
                f"CP {stable_id}: strip/order mismatch")
        anchor = col in (0, 20)
        require(entry["anchor"] == anchor,
                f"CP {stable_id}: anchor mismatch")
        require(entry["contact"], f"CP {stable_id}: contact input false")
        anchor_count += int(entry["anchor"])
        contact_count += int(entry["contact"])
        near_vector(entry["position"], vertices[stable_id],
                    f"CP {stable_id}: imported body position")
    print(f"{PREFIX}OXF-004.RESULT|PASS")
    print(f"{PREFIX}OXF-005.RESULT|PASS")
    require(anchor_count == 14 and contact_count == 147,
            "anchor/contact count mismatch")
    print(f"{PREFIX}OXF-006.RESULT|PASS")

    near_vector(mapped["gravity"], (0, 0, 0), "world gravity")
    near(mapped["conformity"], 0, "conformity")
    near(mapped["collision"], 0, "collision tolerance")
    print(f"{PREFIX}OXF-007.RESULT|PASS")

    near_vector(vertices[73], (0, 0, 0), "center position")
    near_vector(normals[73], (0, 0, 1), "center normal")
    print(f"{PREFIX}OXF-009.RESULT|PASS")
    print(f"{PREFIX}OXF.BODY_VERTEX_COUNT|{len(vertices)}")
    print(f"{PREFIX}OXF.BODY_TRIANGLE_COUNT|{len(triangles)}")
    print(f"{PREFIX}OXF.CP_COUNT|{len(cp)}")
    print(f"{PREFIX}OXF.ANCHOR_COUNT|{anchor_count}")
    print(f"{PREFIX}OXF.CONTACT_INPUT_COUNT|{contact_count}")
    print(f"{PREFIX}OXF.CONTACT_ONLY_UNSUPPORTED_COUNT|{contact_count-anchor_count}")


def negative_tests(doc, source_text):
    mutations = []
    def mutation(name, edit):
        mutated = copy.deepcopy(doc)
        edit(mutated)
        mutations.append((name, lambda m=mutated: import_fixture(m)))

    mutation("FORMAT", lambda d: d.__setitem__("format", "invalid"))
    mutation("DOMAIN", lambda d: d["cloth"]["control_points"][0]
             ["surface_reference"].__setitem__("domain_id", 999))
    mutation("UV", lambda d: d["cloth"]["control_points"][0]
             ["surface_reference"].__setitem__("u", 1.1))
    mutation("TRIANGLE_INDEX", lambda d: d["body_surface"]["triangles"][0]
             .__setitem__(0, 999))
    mutation("DUPLICATE_STABLE_ID", lambda d: d["cloth"]["control_points"][1]
             .__setitem__("stable_id", 0))
    mutation("DUPLICATE_STRIP_ORDER", lambda d: d["cloth"]["control_points"][1]
             .__setitem__("strip_order", 0))
    mutation("ZERO_NORMAL", lambda d: d["body_surface"]["vertices"][0]
             .__setitem__("normal", [0, 0, 0]))
    mutation("CONFORMITY", lambda d: d["inputs"].__setitem__("conformity", 1.1))
    mutation("NEGATIVE_TOLERANCE", lambda d: d["inputs"]
             .__setitem__("collision_tolerance_m", -0.1))
    mutation("VECTOR_LENGTH", lambda d: d["body_surface"]["vertices"][0]
             .__setitem__("position_m", [0, 0]))
    mutation("UNKNOWN_FIELD", lambda d: d.__setitem__("unexpected", True))
    mutations.append(("NONFINITE", lambda: import_fixture(
        parse_json(source_text.replace('"conformity": 0.0',
                                       '"conformity": 1e999')))))
    mutations.append(("DUPLICATE_JSON_KEY", lambda: parse_json(
        '{"format":1,"format":2}')))
    mutations.append(("NONFINITE_CONSTANT", lambda: parse_json(
        '{"value":NaN}')))

    for name, test in mutations:
        try:
            test()
        except (FixtureImportError, ValueError):
            print(f"{PREFIX}OXF-008.{name}|REJECTED")
        else:
            raise FixtureImportError(f"negative case accepted: {name}")
    print(f"{PREFIX}OXF-008.REJECTED_CASE_COUNT|{len(mutations)}")
    print(f"{PREFIX}OXF-008.RESULT|PASS")


def fixture_path():
    candidates = []
    if "__file__" in globals():
        candidates.append(Path(__file__).resolve().parent / "Fixtures" / FILENAME)
    try:
        import azlmbr.paths as o3de_paths
        project_root = getattr(o3de_paths, "projectroot", None)
        if callable(project_root):
            project_root = project_root()
        if project_root:
            candidates.append(Path(str(project_root)) / "Editor" / "Scripts"
                              / "SansaCloth" / "Fixtures" / FILENAME)
    except ImportError:
        pass
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError(
        "deployed Reference fixture missing; checked: "
        + ", ".join(str(p) for p in candidates))


def run():
    try:
        path = fixture_path()
    except FileNotFoundError as exc:
        print(f"{PREFIX}OXF.DISCOVERY|REFERENCE_JSON_NOT_DEPLOYED")
        print(f"{PREFIX}OXF.DETAIL|{exc}")
        print(f"{PREFIX}OXF.RESULT|OPEN")
        return False
    source = path.read_text(encoding="utf-8")
    digest = hashlib.sha256(source.encode("utf-8")).hexdigest()
    doc = parse_json(source)
    mapped = import_fixture(doc)
    print(f"{PREFIX}OXF-001.RESULT|PASS")
    check_import(mapped)
    negative_tests(doc, source)
    print(f"{PREFIX}OXF-010.SOURCE_PATH|{path}")
    print(f"{PREFIX}OXF-010.SHA256|{digest}")
    print(f"{PREFIX}OXF-010.RESULT|PASS")
    print(f"{PREFIX}OXF.CASE_ID|{doc['case_id']}")
    print(f"{PREFIX}OXF.RESULT|PASS")
    return True


try:
    result = run()
except Exception as exc:
    print(f"{PREFIX}OXF.RESULT|FAIL")
    print(f"{PREFIX}OXF.PYTHON_PROBE_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
else:
    print(f"{PREFIX}OXF.PYTHON_PROBE_RESULT|{'PASS' if result else 'OPEN'}")
