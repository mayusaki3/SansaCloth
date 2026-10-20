extends SceneTree

# GSR-007J: evaluate the Flat analytic Surface Query independently at each stage.
# Snapshot surface_position_m / surface_normal are comparison outputs only.
const SNAPSHOT := "res://addons/sansacloth_validation/reference-results/GSR-007H-parity.json"
const EPS := 0.000001
const WIDTH := 0.20
const DEPTH := 0.10

func _vec(a: Array) -> Vector3:
    return Vector3(float(a[0]), float(a[1]), float(a[2]))

func _surface_query(u: float, v: float, current: Vector3) -> Dictionary:
    var surface := Vector3((u - 0.5) * WIDTH, 0.0, (v - 0.5) * DEPTH)
    var normal := Vector3.UP
    return {
        "surface": surface,
        "normal": normal,
        "separation": (current - surface).dot(normal)
    }

func _initialize() -> void:
    if not FileAccess.file_exists(SNAPSHOT):
        printerr("SANSA_GODOT|GSR-007J|MISSING_SNAPSHOT")
        quit(1)
        return
    var document = JSON.parse_string(FileAccess.get_file_as_string(SNAPSHOT))
    if not document is Dictionary or document.get("schema", "") != "GSR-007H-parity-v1":
        printerr("SANSA_GODOT|GSR-007J|INVALID_SNAPSHOT")
        quit(1)
        return
    var cases: Array = document["cases"]
    var expected_cases := ["GSR-007H-C1", "GSR-007H-C2", "GSR-007H-C3", "GSR-007H-C4"]
    if cases.size() != 4:
        printerr("SANSA_GODOT|GSR-007J|CASE_COUNT_ERROR")
        quit(1)
        return
    var tolerance := float(document["collision_tolerance_m"])
    var contact_epsilon := float(document["contact_epsilon_m"])
    var mismatch := 0
    var checked := 0
    var max_position_error := 0.0
    var max_normal_error := 0.0
    var max_separation_error := 0.0
    var seen := {}
    for c in cases:
        var case_id := str(c["case_id"])
        if not expected_cases.has(case_id) or seen.has(case_id):
            printerr("SANSA_GODOT|GSR-007J|CASE_ID_ERROR|", case_id)
            mismatch += 1
        seen[case_id] = true
        var points: Array = c["points"]
        if points.size() != 3:
            printerr("SANSA_GODOT|GSR-007J|POINT_COUNT_ERROR|", case_id)
            quit(1)
            return
        var both_edges := bool(points[0]["anchor"]) and bool(points[2]["anchor"])
        var left := _vec(points[0]["initial_position_m"])
        var right := _vec(points[2]["initial_position_m"])
        for i in range(3):
            var p: Dictionary = points[i]
            if int(p["stable_id"]) != i:
                mismatch += 1
                printerr("SANSA_GODOT|GSR-007J|STABLE_ID_ERROR|", case_id, "|", i)
            var initial := _vec(p["initial_position_m"])
            var bridge := initial
            if both_edges and i == 1:
                bridge = left.lerp(right, 0.5)
            var gravity := bridge
            var conformity := gravity
            var pre := _surface_query(float(i) / 2.0, 0.5, conformity)
            var collision := conformity
            if float(pre["separation"]) < tolerance:
                collision += pre["normal"] * (tolerance - float(pre["separation"]))
            var final_query := _surface_query(float(i) / 2.0, 0.5, collision)
            var contact := float(final_query["separation"]) <= tolerance + contact_epsilon
            var expected_vectors := {
                "bridge": _vec(p["bridge_position_m"]),
                "gravity": _vec(p["gravity_position_m"]),
                "conformity": _vec(p["conformity_position_m"]),
                "collision": _vec(p["collision_position_m"]),
                "final": _vec(p["final_position_m"]),
                "surface": _vec(p["surface_position_m"])
            }
            var actual_vectors := {
                "bridge": bridge,
                "gravity": gravity,
                "conformity": conformity,
                "collision": collision,
                "final": collision,
                "surface": final_query["surface"]
            }
            for key in expected_vectors.keys():
                var error: float = actual_vectors[key].distance_to(expected_vectors[key])
                max_position_error = maxf(max_position_error, error)
                if error > EPS:
                    printerr("SANSA_GODOT|GSR-007J|POSITION_MISMATCH|", case_id, "|", i, "|", key, "|", error)
                    mismatch += 1
            var normal_error: float = final_query["normal"].distance_to(_vec(p["surface_normal"]))
            var separation_error: float = absf(float(final_query["separation"]) - float(p["separation_m"]))
            max_normal_error = maxf(max_normal_error, normal_error)
            max_separation_error = maxf(max_separation_error, separation_error)
            if normal_error > EPS or separation_error > EPS:
                printerr("SANSA_GODOT|GSR-007J|SURFACE_MISMATCH|", case_id, "|", i)
                mismatch += 1
            if contact != bool(p["contact"]):
                printerr("SANSA_GODOT|GSR-007J|CONTACT_MISMATCH|", case_id, "|", i)
                mismatch += 1
            var support := "Anchor" if bool(p["anchor"]) else "Unsupported"
            if support != str(p["support"]):
                printerr("SANSA_GODOT|GSR-007J|SUPPORT_MISMATCH|", case_id, "|", i)
                mismatch += 1
            checked += 1
    print("SANSA_GODOT|GSR-007J|CASES|", cases.size())
    print("SANSA_GODOT|GSR-007J|POINTS|", checked)
    print("SANSA_GODOT|GSR-007J|MAX_POSITION_ERROR_M|", max_position_error)
    print("SANSA_GODOT|GSR-007J|MAX_NORMAL_ERROR|", max_normal_error)
    print("SANSA_GODOT|GSR-007J|MAX_SEPARATION_ERROR_M|", max_separation_error)
    print("SANSA_GODOT|GSR-007J|MISMATCHES|", mismatch)
    if mismatch > 0:
        quit(1)
        return
    print("SANSA_GODOT|GSR-007J|PASS")
    quit(0)
