extends SceneTree

# GSR-007C: independent flat, BothEdges gravity-stage probe.
# Scope: SR-001-C1-G1 only; not general SurfaceResponse solver parity.
const FIXTURE := "res://addons/sansacloth_validation/fixtures/SR-001-C1-G1.json"
const REFERENCE := "res://addons/sansacloth_validation/reference-results/SR-001-C1-G1.json"
const EPS := 0.000001

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007C|FAIL|", reason)
    quit(1)

func _read(path: String) -> Variant:
    if not FileAccess.file_exists(path):
        return null
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return null
    return JSON.parse_string(file.get_as_text())

func _vector(a: Array) -> Vector3:
    return Vector3(float(a[0]), float(a[1]), float(a[2]))

func _initialize() -> void:
    var fixture = _read(FIXTURE)
    var result = _read(REFERENCE)
    if not (fixture is Dictionary) or not (result is Dictionary):
        _fail("MISSING_JSON")
        return
    if fixture.get("case_id") != "SR-001-C1-G1" or result.get("case_id") != "SR-001-C1-G1":
        _fail("CASE_ID")
        return
    var profile: Dictionary = result.get("profile", {})
    if profile.get("support_layout") != "BothEdges":
        _fail("UNSUPPORTED_LAYOUT")
        return
    var inputs: Dictionary = fixture.get("inputs", {})
    var gravity := _vector(inputs.get("world_gravity_m_per_s2", [0, 0, 0]))
    if gravity.length() < EPS or float(inputs.get("conformity", -1)) != 1.0:
        _fail("INPUTS")
        return
    var cloth: Dictionary = fixture.get("cloth", {})
    var points: Array = cloth.get("control_points", [])
    var expected: Array = result.get("control_points", [])
    if points.size() != 147 or expected.size() != 147:
        _fail("COUNTS")
        return
    var by_id := {}
    for entry in expected:
        var id := int(entry.get("stable_id", -1))
        if id < 0 or by_id.has(id):
            _fail("REFERENCE_IDS")
            return
        by_id[id] = entry
    # Flat fixture has support at x=-0.1 and x=+0.1 on every strip.
    # Reference's gravity sag follows 4*t*(1-t) with a 0.01 m peak.
    var x_min := INF
    var x_max := -INF
    for point in points:
        if point.get("anchor") == true:
            var x := float(point["position_m"][0])
            x_min = minf(x_min, x)
            x_max = maxf(x_max, x)
    var span := x_max - x_min
    if span <= EPS:
        _fail("SUPPORT_SPAN")
        return
    var amplitude := float(profile.get("characteristic_length_m", 0)) * float(profile.get("quasi_static_gravity_scale", 0))
    var mismatches := 0
    var max_gravity_error := 0.0
    var max_collision_error := 0.0
    var nonzero_sag := 0
    for point in points:
        var id := int(point.get("stable_id", -1))
        if not by_id.has(id):
            _fail("POINT_ID")
            return
        var ref: Dictionary = by_id[id]
        var original := _vector(point["position_m"])
        var t := clampf((original.x - x_min) / span, 0.0, 1.0)
        var weight := 4.0 * t * (1.0 - t)
        var gravity_position := original + gravity.normalized() * (amplitude * weight)
        if point.get("anchor") == true:
            gravity_position = original
        if gravity_position.distance_to(original) > EPS:
            nonzero_sag += 1
        var gravity_error := gravity_position.distance_to(_vector(ref["gravity_position_m"]))
        # Flat surface at y=0 with upward normal; zero tolerance.
        var corrected := gravity_position
        if corrected.y < 0.0:
            corrected.y = 0.0
        var collision_error := corrected.distance_to(_vector(ref["collision_position_m"]))
        max_gravity_error = maxf(max_gravity_error, gravity_error)
        max_collision_error = maxf(max_collision_error, collision_error)
        if gravity_error > EPS or collision_error > EPS:
            mismatches += 1
            if mismatches <= 5:
                printerr("SANSA_GODOT|GSR-007C|MISMATCH|ID=", id, "|GRAVITY=", gravity_error, "|COLLISION=", collision_error)
    print("SANSA_GODOT|GSR-007C|CHECKED|", points.size())
    print("SANSA_GODOT|GSR-007C|NONZERO_SAG|", nonzero_sag)
    print("SANSA_GODOT|GSR-007C|MAX_GRAVITY_ERROR_M|", max_gravity_error)
    print("SANSA_GODOT|GSR-007C|MAX_COLLISION_ERROR_M|", max_collision_error)
    print("SANSA_GODOT|GSR-007C|MISMATCHES|", mismatches)
    if nonzero_sag == 0 or mismatches != 0:
        _fail("STAGE_MISMATCH")
        return
    print("SANSA_GODOT|GSR-007C|PASS")
    quit(0)
