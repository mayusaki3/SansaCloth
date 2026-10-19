extends SceneTree

# GSR-007H: independent data-driven stage parity against Rust Reference snapshot.
const SNAPSHOT := "res://addons/sansacloth_validation/reference-results/GSR-007H-parity.json"
const EPS := 0.000001

func _vec(value: Array) -> Vector3:
    return Vector3(float(value[0]), float(value[1]), float(value[2]))

func _compare_vector(label: String, actual: Vector3, expected: Vector3) -> int:
    var error := actual.distance_to(expected)
    if error > EPS:
        printerr("SANSA_GODOT|GSR-007H-PARITY|MISMATCH|", label, "|ERROR_M|", error)
        return 1
    return 0

func _initialize() -> void:
    if not FileAccess.file_exists(SNAPSHOT):
        printerr("SANSA_GODOT|GSR-007H-PARITY|MISSING_SNAPSHOT|", SNAPSHOT)
        quit(1)
        return
    var document = JSON.parse_string(FileAccess.get_file_as_string(SNAPSHOT))
    if not document is Dictionary or document.get("schema", "") != "GSR-007H-parity-v1":
        printerr("SANSA_GODOT|GSR-007H-PARITY|INVALID_SNAPSHOT")
        quit(1)
        return
    var tolerance := float(document["collision_tolerance_m"])
    var contact_epsilon := float(document["contact_epsilon_m"])
    var cases: Array = document["cases"]
    var mismatches := 0
    var checked := 0
    if cases.size() != 4:
        printerr("SANSA_GODOT|GSR-007H-PARITY|INVALID_CASE_COUNT")
        quit(1)
        return
    for case in cases:
        var case_id := str(case["case_id"])
        var points: Array = case["points"]
        if points.size() != 3:
            printerr("SANSA_GODOT|GSR-007H-PARITY|INVALID_POINT_COUNT|", case_id)
            quit(1)
            return
        var both_edges := bool(points[0]["anchor"]) and bool(points[2]["anchor"])
        var left := _vec(points[0]["initial_position_m"])
        var right := _vec(points[2]["initial_position_m"])
        for i in range(3):
            var point: Dictionary = points[i]
            var label := case_id + "|POINT|" + str(i)
            if int(point["stable_id"]) != i:
                printerr("SANSA_GODOT|GSR-007H-PARITY|ID_MISMATCH|", label)
                mismatches += 1
            var initial := _vec(point["initial_position_m"])
            var surface := _vec(point["surface_position_m"])
            var normal := _vec(point["surface_normal"])
            var bridge := initial
            if both_edges and i > 0 and i < 2:
                bridge = left.lerp(right, float(i) / 2.0)
            var gravity := bridge # Zero gravity
            var conformity := gravity # Zero conformity
            var separation_before := (conformity - surface).dot(normal)
            var collision := conformity
            if separation_before < tolerance:
                collision += normal.normalized() * (tolerance - separation_before)
            var final_position := collision
            var separation := (final_position - surface).dot(normal)
            var contact := separation <= tolerance + contact_epsilon
            mismatches += _compare_vector(label + "|BRIDGE", bridge, _vec(point["bridge_position_m"]))
            mismatches += _compare_vector(label + "|GRAVITY", gravity, _vec(point["gravity_position_m"]))
            mismatches += _compare_vector(label + "|CONFORMITY", conformity, _vec(point["conformity_position_m"]))
            mismatches += _compare_vector(label + "|COLLISION", collision, _vec(point["collision_position_m"]))
            mismatches += _compare_vector(label + "|FINAL", final_position, _vec(point["final_position_m"]))
            if absf(separation - float(point["separation_m"])) > EPS:
                printerr("SANSA_GODOT|GSR-007H-PARITY|SEPARATION_MISMATCH|", label)
                mismatches += 1
            if contact != bool(point["contact"]):
                printerr("SANSA_GODOT|GSR-007H-PARITY|CONTACT_MISMATCH|", label)
                mismatches += 1
            var support := "Anchor" if bool(point["anchor"]) else "Unsupported"
            if support != str(point["support"]):
                printerr("SANSA_GODOT|GSR-007H-PARITY|SUPPORT_MISMATCH|", label)
                mismatches += 1
            checked += 1
    print("SANSA_GODOT|GSR-007H-PARITY|CASES|", cases.size())
    print("SANSA_GODOT|GSR-007H-PARITY|POINTS|", checked)
    print("SANSA_GODOT|GSR-007H-PARITY|MISMATCHES|", mismatches)
    if mismatches > 0:
        quit(1)
        return
    print("SANSA_GODOT|GSR-007H-PARITY|PASS")
    quit(0)
