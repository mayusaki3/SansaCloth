extends SceneTree

# GSR-007B baseline only: SR-001-C0-G0, zero gravity and conformity.
# This checks flat-surface correction against independent Rust reference output.
# It does not establish general SurfaceResponse parity.
const INPUT_PATH := "res://addons/sansacloth_validation/fixtures/SR-001-C0-G0.json"
const EXPECTED_PATH := "res://addons/sansacloth_validation/reference-results/SR-001-C0-G0.json"
const SOLVER_PATH := "res://addons/sansacloth_validation/flat_collision_solver.gd"
const EPSILON := 0.000001

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007B|FAIL|", reason)
    quit(1)

func _read_json(path: String) -> Variant:
    if not FileAccess.file_exists(path):
        return null
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return null
    return JSON.parse_string(file.get_as_text())

func _vec3(value: Variant) -> Vector3:
    return Vector3(float(value[0]), float(value[1]), float(value[2]))

func _initialize() -> void:
    var fixture = _read_json(INPUT_PATH)
    var expected = _read_json(EXPECTED_PATH)
    if not (fixture is Dictionary) or not (expected is Dictionary):
        _fail("MISSING_OR_INVALID_JSON")
        return
    if fixture.get("case_id") != "SR-001-C0-G0" or expected.get("case_id") != "SR-001-C0-G0":
        _fail("CASE_ID")
        return
    if expected.get("format") != "sansacloth.validation.surface-response-result/0":
        _fail("RESULT_FORMAT")
        return
    var inputs: Dictionary = fixture.get("inputs", {})
    var gravity = inputs.get("world_gravity_m_per_s2")
    if gravity != [0.0, 0.0, 0.0] or float(inputs.get("conformity", -1)) != 0.0:
        _fail("UNSUPPORTED_NONZERO_INPUT")
        return
    var body: Dictionary = fixture.get("body_surface", {})
    var vertices: Array = body.get("vertices", [])
    var cloth: Dictionary = fixture.get("cloth", {})
    var points: Array = cloth.get("control_points", [])
    var reference_points: Array = expected.get("control_points", [])
    if vertices.size() != 147 or points.size() != 147 or reference_points.size() != 147:
        _fail("POINT_COUNTS")
        return
    var references := {}
    for entry in reference_points:
        var id := int(entry.get("stable_id", -1))
        if id < 0 or references.has(id):
            _fail("REFERENCE_IDS")
            return
        references[id] = entry
    if not ResourceLoader.exists(SOLVER_PATH):
        _fail("SOLVER_MISSING")
        return
    var solver = load(SOLVER_PATH).new()
    var mismatches := 0
    var max_position_error := 0.0
    var max_normal_error := 0.0
    var max_separation_error := 0.0
    var checked := 0
    for point in points:
        var id := int(point.get("stable_id", -1))
        if not references.has(id):
            _fail("MISSING_REFERENCE_ID")
            return
        var reference: Dictionary = references[id]
        var surface_reference: Dictionary = point.get("surface_reference", {})
        var u := float(surface_reference.get("u", -1))
        var v := float(surface_reference.get("v", -1))
        # The basic flat fixture has coincident UV samples on its body grid.
        var surface_vertex: Dictionary = {}
        for vertex in vertices:
            var uv: Array = vertex.get("uv", [])
            if uv.size() == 2 and absf(float(uv[0]) - u) <= EPSILON and absf(float(uv[1]) - v) <= EPSILON:
                surface_vertex = vertex
                break
        if surface_vertex.is_empty():
            _fail("SURFACE_UV_NOT_FOUND")
            return
        var surface_position := _vec3(surface_vertex["position_m"])
        var normal := _vec3(surface_vertex["normal"]).normalized()
        var original := _vec3(point["position_m"])
        var corrected: Vector3 = solver.correct_point(original, surface_position, normal, float(inputs["collision_tolerance_m"]))
        var separation := (corrected - surface_position).dot(normal)
        var contact := bool(point.get("contact", false))
        var position_error := corrected.distance_to(_vec3(reference["final_position_m"]))
        var normal_error := normal.distance_to(_vec3(reference["surface_normal"]))
        var separation_error := absf(separation - float(reference["separation_m"]))
        max_position_error = maxf(max_position_error, position_error)
        max_normal_error = maxf(max_normal_error, normal_error)
        max_separation_error = maxf(max_separation_error, separation_error)
        if position_error > EPSILON or normal_error > EPSILON or separation_error > EPSILON or contact != bool(reference["contact"]):
            mismatches += 1
            if mismatches <= 5:
                printerr("SANSA_GODOT|GSR-007B|MISMATCH|ID=", id, "|POSITION=", position_error, "|NORMAL=", normal_error, "|SEPARATION=", separation_error)
        checked += 1
    print("SANSA_GODOT|GSR-007B|CHECKED|", checked)
    print("SANSA_GODOT|GSR-007B|MAX_POSITION_ERROR_M|", max_position_error)
    print("SANSA_GODOT|GSR-007B|MAX_NORMAL_ERROR|", max_normal_error)
    print("SANSA_GODOT|GSR-007B|MAX_SEPARATION_ERROR_M|", max_separation_error)
    print("SANSA_GODOT|GSR-007B|MISMATCHES|", mismatches)
    if mismatches != 0:
        _fail("NUMERICAL_MISMATCH")
        return
    print("SANSA_GODOT|GSR-007B|PASS")
    quit(0)
