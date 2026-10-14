extends SceneTree

# GSR-007A: fixture import and validation only, NOT numerical solver parity.
const FIXTURE_PATH := "res://addons/sansacloth_validation/fixtures/SR-001-C0-G0.json"

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007A|FAIL|", reason)
    quit(1)

func _initialize() -> void:
    if not FileAccess.file_exists(FIXTURE_PATH):
        _fail("FIXTURE_MISSING")
        return
    var file := FileAccess.open(FIXTURE_PATH, FileAccess.READ)
    if file == null:
        _fail("READ_ERROR")
        return
    var parsed = JSON.parse_string(file.get_as_text())
    if not (parsed is Dictionary):
        _fail("INVALID_JSON")
        return
    var doc: Dictionary = parsed
    if doc.get("format") != "sansacloth.validation.fixture-exchange/0" or doc.get("case_id") != "SR-001-C0-G0":
        _fail("FORMAT_OR_CASE")
        return
    var coordinate: Dictionary = doc.get("coordinate", {})
    if coordinate.get("length_unit") != "m" or coordinate.get("gravity_unit") != "m/s^2" or coordinate.get("x") != "right" or coordinate.get("y") != "up" or coordinate.get("z") != "forward":
        _fail("COORDINATE")
        return
    var body: Dictionary = doc.get("body_surface", {})
    var vertices: Array = body.get("vertices", [])
    var triangles: Array = body.get("triangles", [])
    var cloth: Dictionary = doc.get("cloth", {})
    var points: Array = cloth.get("control_points", [])
    if body.get("domain_id") != 1 or vertices.size() != 147 or triangles.size() != 240 or points.size() != 147:
        _fail("COUNTS_OR_DOMAIN")
        return
    var ids := {}
    var anchor_count := 0
    for point in points:
        if not (point is Dictionary):
            _fail("POINT_TYPE")
            return
        var id = point.get("stable_id", -1)
        if not (id is float or id is int) or int(id) != id or id < 0 or id >= 147 or ids.has(int(id)):
            _fail("STABLE_ID")
            return
        ids[int(id)] = true
        if point.get("anchor") == true:
            anchor_count += 1
        var position = point.get("position_m", [])
        var reference: Dictionary = point.get("surface_reference", {})
        if not (position is Array) or position.size() != 3 or reference.get("domain_id") != 1:
            _fail("POINT_SCHEMA")
            return
    if anchor_count != 14:
        _fail("ANCHOR_COUNT")
        return
    for triangle in triangles:
        if not (triangle is Array) or triangle.size() != 3:
            _fail("TRIANGLE_SCHEMA")
            return
        for index in triangle:
            if not (index is float or index is int) or int(index) != index or index < 0 or index >= vertices.size():
                _fail("TRIANGLE_INDEX")
                return
    var inputs: Dictionary = doc.get("inputs", {})
    var gravity = inputs.get("world_gravity_m_per_s2")
    if not (gravity is Array) or gravity.size() != 3:
        _fail("GRAVITY_SCHEMA")
        return
    for component in gravity:
        if not (component is float or component is int) or absf(float(component)) > 0.000000001:
            _fail("GRAVITY_VALUE")
            return
    for key in ["conformity", "collision_tolerance_m"]:
        var value = inputs.get(key)
        if not (value is float or value is int) or absf(float(value)) > 0.000000001:
            _fail("INPUT_" + key.to_upper())
            return
    print("SANSA_GODOT|GSR-007A|VERTEX_COUNT|", vertices.size())
    print("SANSA_GODOT|GSR-007A|TRIANGLE_COUNT|", triangles.size())
    print("SANSA_GODOT|GSR-007A|CP_COUNT|", points.size())
    print("SANSA_GODOT|GSR-007A|ANCHOR_COUNT|", anchor_count)
    print("SANSA_GODOT|GSR-007A|PASS")
    quit(0)
