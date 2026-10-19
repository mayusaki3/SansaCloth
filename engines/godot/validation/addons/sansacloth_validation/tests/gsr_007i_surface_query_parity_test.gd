extends SceneTree

# Independent analytic evaluation. Rust expected surface data is never used as input.
const SNAPSHOT := "res://addons/sansacloth_validation/reference-results/GSR-007I-surface-query.json"
const WIDTH := 0.20
const DEPTH := 0.10
const FEATURE_WIDTH := 0.10

func _vec(v: Array) -> Vector3:
    return Vector3(float(v[0]), float(v[1]), float(v[2]))

func _query(kind: String, u: float, v: float, current: Vector3) -> Dictionary:
    var x := (u - 0.5) * WIDTH
    var z := (v - 0.5) * DEPTH
    var amplitude := 0.0
    match kind:
        "flat":
            amplitude = 0.0
        "convex":
            amplitude = 0.03
        "concave_shallow":
            amplitude = -0.02
        "concave_deep":
            amplitude = -0.05
        _:
            return {}
    var y := 0.0
    var derivative := 0.0
    if absf(x) <= FEATURE_WIDTH * 0.5:
        var phase := 2.0 * PI * x / FEATURE_WIDTH
        y = amplitude * 0.5 * (1.0 + cos(phase))
        derivative = -(amplitude * PI / FEATURE_WIDTH) * sin(phase)
    var surface := Vector3(x, y, z)
    var normal := Vector3(-derivative, 1.0, 0.0).normalized()
    return {
        "position": surface,
        "normal": normal,
        "separation": (current - surface).dot(normal)
    }

func _initialize() -> void:
    if not FileAccess.file_exists(SNAPSHOT):
        printerr("SANSA_GODOT|GSR-007I|MISSING_SNAPSHOT|", SNAPSHOT)
        quit(1)
        return
    var data = JSON.parse_string(FileAccess.get_file_as_string(SNAPSHOT))
    if not data is Dictionary or data.get("schema", "") != "GSR-007I-surface-query-v1":
        printerr("SANSA_GODOT|GSR-007I|INVALID_SNAPSHOT")
        quit(1)
        return
    var probes: Array = data["probes"]
    if probes.size() != 168:
        printerr("SANSA_GODOT|GSR-007I|INVALID_PROBE_COUNT|", probes.size())
        quit(1)
        return
    var tolerance := float(data["tolerance"])
    var mismatches := 0
    var max_position_error := 0.0
    var max_normal_error := 0.0
    var max_separation_error := 0.0
    var kinds := {}
    for i in range(probes.size()):
        var probe: Dictionary = probes[i]
        if int(probe["stable_id"]) != i:
            printerr("SANSA_GODOT|GSR-007I|ID_MISMATCH|", i)
            mismatches += 1
        var kind := str(probe["fixture_kind"])
        kinds[kind] = true
        var result := _query(kind, float(probe["u"]), float(probe["v"]), _vec(probe["current_position_m"]))
        if result.is_empty():
            printerr("SANSA_GODOT|GSR-007I|UNKNOWN_FIXTURE|", kind)
            mismatches += 1
            continue
        var position_error: float = result["position"].distance_to(_vec(probe["surface_position_m"]))
        var normal_error: float = result["normal"].distance_to(_vec(probe["surface_normal"]))
        var separation_error: float = absf(float(result["separation"]) - float(probe["separation_m"]))
        max_position_error = maxf(max_position_error, position_error)
        max_normal_error = maxf(max_normal_error, normal_error)
        max_separation_error = maxf(max_separation_error, separation_error)
        if position_error > tolerance or normal_error > tolerance or separation_error > tolerance:
            printerr("SANSA_GODOT|GSR-007I|MISMATCH|", i, "|P|", position_error, "|N|", normal_error, "|S|", separation_error)
            mismatches += 1
    if kinds.size() != 4:
        printerr("SANSA_GODOT|GSR-007I|FIXTURE_COUNT_MISMATCH|", kinds.size())
        mismatches += 1
    print("SANSA_GODOT|GSR-007I|PROBES|", probes.size())
    print("SANSA_GODOT|GSR-007I|MAX_POSITION_ERROR_M|", max_position_error)
    print("SANSA_GODOT|GSR-007I|MAX_NORMAL_ERROR|", max_normal_error)
    print("SANSA_GODOT|GSR-007I|MAX_SEPARATION_ERROR_M|", max_separation_error)
    print("SANSA_GODOT|GSR-007I|MISMATCHES|", mismatches)
    if mismatches != 0:
        quit(1)
        return
    print("SANSA_GODOT|GSR-007I|PASS")
    quit(0)
