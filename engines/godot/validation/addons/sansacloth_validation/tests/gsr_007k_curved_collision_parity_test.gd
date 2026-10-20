extends SceneTree

# GSR-007K independent curved analytic query and Collision stage.
const SNAPSHOT := "res://addons/sansacloth_validation/reference-results/GSR-007K-curved-collision.json"
const WIDTH := 0.20
const DEPTH := 0.10
const FEATURE_WIDTH := 0.10

func _vec(a: Array) -> Vector3:
    return Vector3(float(a[0]), float(a[1]), float(a[2]))

func _query(kind: String, u: float, v: float, current: Vector3) -> Dictionary:
    var amplitude := 0.0
    match kind:
        "convex": amplitude = 0.03
        "concave_shallow": amplitude = -0.02
        "concave_deep": amplitude = -0.05
        _: return {}
    var x := (u - 0.5) * WIDTH
    var z := (v - 0.5) * DEPTH
    var y := 0.0
    var dydx := 0.0
    if absf(x) <= FEATURE_WIDTH * 0.5:
        var phase := 2.0 * PI * x / FEATURE_WIDTH
        y = amplitude * 0.5 * (1.0 + cos(phase))
        dydx = -(amplitude * PI / FEATURE_WIDTH) * sin(phase)
    var surface := Vector3(x, y, z)
    var normal := Vector3(-dydx, 1.0, 0.0).normalized()
    return {
        "surface": surface,
        "normal": normal,
        "separation": (current - surface).dot(normal)
    }

func _initialize() -> void:
    if not FileAccess.file_exists(SNAPSHOT):
        printerr("SANSA_GODOT|GSR-007K|MISSING_SNAPSHOT")
        quit(1)
        return
    var doc = JSON.parse_string(FileAccess.get_file_as_string(SNAPSHOT))
    if not doc is Dictionary or doc.get("schema", "") != "GSR-007K-curved-collision-v1":
        printerr("SANSA_GODOT|GSR-007K|INVALID_SNAPSHOT")
        quit(1)
        return
    var probes: Array = doc["probes"]
    if probes.size() != 81:
        printerr("SANSA_GODOT|GSR-007K|INVALID_PROBE_COUNT|", probes.size())
        quit(1)
        return
    var tolerance := float(doc["collision_tolerance_m"])
    var contact_epsilon := float(doc["contact_epsilon_m"])
    var eps := float(doc["comparison_epsilon"])
    var mismatches := 0
    var kinds := {}
    var max_position_error := 0.0
    var max_normal_error := 0.0
    var max_separation_error := 0.0
    for i in range(probes.size()):
        var p: Dictionary = probes[i]
        if int(p["stable_id"]) != i:
            printerr("SANSA_GODOT|GSR-007K|ID_MISMATCH|", i)
            mismatches += 1
        var kind := str(p["fixture_kind"])
        kinds[kind] = true
        var initial := _vec(p["initial_position_m"])
        var before := _query(kind, float(p["u"]), float(p["v"]), initial)
        if before.is_empty():
            printerr("SANSA_GODOT|GSR-007K|UNKNOWN_FIXTURE|", kind)
            mismatches += 1
            continue
        var corrected := initial
        if float(before["separation"]) < tolerance:
            corrected += before["normal"] * (tolerance - float(before["separation"]))
        var after := _query(kind, float(p["u"]), float(p["v"]), corrected)
        var actual_vectors := {
            "before_surface_position_m": before["surface"],
            "before_surface_normal": before["normal"],
            "collision_position_m": corrected,
            "after_surface_position_m": after["surface"],
            "after_surface_normal": after["normal"]
        }
        for field in actual_vectors.keys():
            var error: float = actual_vectors[field].distance_to(_vec(p[field]))
            if field.contains("normal"):
                max_normal_error = maxf(max_normal_error, error)
            else:
                max_position_error = maxf(max_position_error, error)
            if error > eps:
                printerr("SANSA_GODOT|GSR-007K|VECTOR_MISMATCH|", i, "|", field, "|", error)
                mismatches += 1
        for field in ["before_separation_m", "after_separation_m"]:
            var actual: float = before["separation"] if field == "before_separation_m" else after["separation"]
            var error := absf(actual - float(p[field]))
            max_separation_error = maxf(max_separation_error, error)
            if error > eps:
                printerr("SANSA_GODOT|GSR-007K|SEPARATION_MISMATCH|", i, "|", field, "|", error)
                mismatches += 1
        var contact := float(after["separation"]) <= tolerance + contact_epsilon
        if contact != bool(p["contact"]):
            printerr("SANSA_GODOT|GSR-007K|CONTACT_MISMATCH|", i)
            mismatches += 1
    if kinds.size() != 3 or not kinds.has("convex") or not kinds.has("concave_shallow") or not kinds.has("concave_deep"):
        printerr("SANSA_GODOT|GSR-007K|FIXTURE_SET_MISMATCH")
        mismatches += 1
    print("SANSA_GODOT|GSR-007K|PROBES|", probes.size())
    print("SANSA_GODOT|GSR-007K|MAX_POSITION_ERROR_M|", max_position_error)
    print("SANSA_GODOT|GSR-007K|MAX_NORMAL_ERROR|", max_normal_error)
    print("SANSA_GODOT|GSR-007K|MAX_SEPARATION_ERROR_M|", max_separation_error)
    print("SANSA_GODOT|GSR-007K|MISMATCHES|", mismatches)
    if mismatches != 0:
        quit(1)
        return
    print("SANSA_GODOT|GSR-007K|PASS")
    quit(0)
