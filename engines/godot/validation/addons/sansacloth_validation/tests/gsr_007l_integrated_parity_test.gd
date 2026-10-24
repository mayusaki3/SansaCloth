extends SceneTree

const SNAPSHOT := "res://addons/sansacloth_validation/reference-results/GSR-007L-integrated.json"
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
    return {"surface": surface, "normal": normal, "separation": (current - surface).dot(normal)}

func _initialize() -> void:
    if not FileAccess.file_exists(SNAPSHOT):
        printerr("SANSA_GODOT|GSR-007L|FAIL|MISSING_SNAPSHOT")
        quit(1)
        return
    var doc = JSON.parse_string(FileAccess.get_file_as_string(SNAPSHOT))
    if not (doc is Dictionary) or doc.get("schema", "") != "GSR-007L-integrated-v1":
        printerr("SANSA_GODOT|GSR-007L|FAIL|INVALID_SCHEMA")
        quit(1)
        return
    var cases: Array = doc.get("cases", [])
    if cases.size() != 36:
        printerr("SANSA_GODOT|GSR-007L|FAIL|CASE_COUNT|", cases.size())
        quit(1)
        return
    var eps := float(doc["comparison_epsilon"])
    var reach := float(doc["conformity_reach_m"])
    var tolerance := float(doc["collision_tolerance_m"])
    var contact_eps := float(doc["contact_epsilon_m"])
    var gravity_scale := float(doc["quasi_static_gravity_scale"])
    var length_m := float(doc["characteristic_length_m"])
    if eps <= 0.0 or reach != 0.02 or tolerance != 0.005 or contact_eps != 0.000001 or gravity_scale != 0.1 or length_m != 0.10:
        printerr("SANSA_GODOT|GSR-007L|FAIL|SETTINGS")
        quit(1)
        return
    var mismatch := 0
    var checked := 0
    var contacts := 0
    var gravity_moved := false
    var conformity_moved := false
    var max_errors := {}
    var case_ids := {}
    var kinds := {}
    var layouts := {}
    var gravities := {}
    var conformities := {}
    for case in cases:
        var case_id := str(case.get("case_id", ""))
        var kind := str(case.get("fixture_kind", ""))
        var layout := str(case.get("support_layout", ""))
        var gravity := _vec(case["gravity_m_per_s2"])
        var conformity := float(case["conformity"])
        var gravity_on := gravity.length_squared() > 0.0
        var gravity_id := 1 if gravity_on else 0
        var conformity_id := 0 if conformity == 0.0 else (5 if conformity == 0.5 else 10)
        var layout_id := "both" if layout == "BothEdges" else "one"
        var expected_id := "GSR-007L-%s-%s-G%d-C%d" % [kind, layout_id, gravity_id, conformity_id]
        if case_ids.has(case_id) or case_id != expected_id or (layout != "BothEdges" and layout != "OneEdge") or (conformity != 0.0 and conformity != 0.5 and conformity != 1.0) or (gravity_on and gravity.distance_to(Vector3(0.0, -9.80665, 0.0)) > eps):
            printerr("SANSA_GODOT|GSR-007L|FAIL|CASE_PARAMETERS|", case_id)
            quit(1)
            return
        case_ids[case_id] = true
        kinds[kind] = true
        layouts[layout] = true
        gravities[gravity_id] = true
        conformities[conformity_id] = true
        var points: Array = case.get("points", [])
        if points.size() != 9:
            printerr("SANSA_GODOT|GSR-007L|FAIL|POINT_COUNT|", case_id)
            quit(1)
            return
        var initial: Array[Vector3] = []
        var anchor: Array[bool] = []
        var seen := {}
        for i in range(9):
            var p: Dictionary = points[i]
            var id := int(p.get("stable_id", -1))
            if id != i or seen.has(id) or absf(float(p["v"]) - 0.5) > eps:
                printerr("SANSA_GODOT|GSR-007L|FAIL|POINT_ID|", case_id, "|", i)
                quit(1)
                return
            seen[id] = true
            initial.append(_vec(p["initial_position_m"]))
            anchor.append(bool(p["anchor"]))
            if anchor[i] != (i == 0 or (layout == "BothEdges" and i == 8)):
                printerr("SANSA_GODOT|GSR-007L|FAIL|ANCHOR|", case_id, "|", i)
                quit(1)
                return
        var bridge: Array[Vector3] = initial.duplicate()
        if anchor[0] and anchor[8]:
            for i in range(1, 8):
                bridge[i] = initial[0].lerp(initial[8], float(i) / 8.0)
        var gravity_positions: Array[Vector3] = bridge.duplicate()
        if gravity_on:
            var direction := gravity.normalized()
            for i in range(9):
                if anchor[i]:
                    continue
                var t := float(i) / 8.0
                var weight := 4.0 * t * (1.0 - t) if layout == "BothEdges" else t
                gravity_positions[i] += direction * length_m * gravity_scale * weight
        for i in range(9):
            var p: Dictionary = points[i]
            var u := float(p["u"])
            var v := float(p["v"])
            var support := "Anchor" if anchor[i] else "Unsupported"
            if str(p["support"]) != support:
                mismatch += 1
                printerr("SANSA_GODOT|GSR-007L|SUPPORT_MISMATCH|", case_id, "|", i)
            var qg := _query(kind, u, v, gravity_positions[i])
            if qg.is_empty():
                printerr("SANSA_GODOT|GSR-007L|FAIL|FIXTURE|", kind)
                quit(1)
                return
            var conform := gravity_positions[i]
            var separation := float(qg["separation"])
            if not anchor[i] and bool(p["supported_strip"]) and conformity > 0.0 and reach > 0.0 and separation >= 0.0 and separation < reach:
                var weight := clampf(1.0 - separation / reach, 0.0, 1.0)
                conform = gravity_positions[i].lerp(qg["surface"], conformity * weight)
            var qc := _query(kind, u, v, conform)
            var collision := conform
            if float(qc["separation"]) < tolerance:
                collision += qc["normal"] * (tolerance - float(qc["separation"]))
            var qf := _query(kind, u, v, collision)
            var vectors := {
                "bridge_position_m": bridge[i],
                "gravity_position_m": gravity_positions[i],
                "conformity_position_m": conform,
                "collision_position_m": collision,
                "final_position_m": collision,
                "surface_position_m": qf["surface"],
                "surface_normal": qf["normal"]
            }
            for field in vectors.keys():
                var err: float = vectors[field].distance_to(_vec(p[field]))
                max_errors[field] = maxf(float(max_errors.get(field, 0.0)), err)
                if err > eps:
                    mismatch += 1
                    if mismatch <= 12:
                        printerr("SANSA_GODOT|GSR-007L|VECTOR_MISMATCH|", case_id, "|", i, "|", field, "|", err)
            var sep_err := absf(float(qf["separation"]) - float(p["separation_m"]))
            max_errors["separation_m"] = maxf(float(max_errors.get("separation_m", 0.0)), sep_err)
            if sep_err > eps:
                mismatch += 1
                if mismatch <= 12:
                    printerr("SANSA_GODOT|GSR-007L|SEPARATION_MISMATCH|", case_id, "|", i, "|", sep_err)
            var contact := float(qf["separation"]) <= tolerance + contact_eps
            if contact != bool(p["contact"]):
                mismatch += 1
                if mismatch <= 12:
                    printerr("SANSA_GODOT|GSR-007L|CONTACT_MISMATCH|", case_id, "|", i, "|", float(qf["separation"]))
            if contact:
                contacts += 1
            if gravity_positions[i].distance_to(bridge[i]) > 0.000000001:
                gravity_moved = true
            if conform.distance_to(gravity_positions[i]) > 0.000000001:
                conformity_moved = true
            checked += 1
    if case_ids.size() != 36 or checked != 324 or kinds.size() != 3 or layouts.size() != 2 or gravities.size() != 2 or conformities.size() != 3 or not gravity_moved or not conformity_moved:
        printerr("SANSA_GODOT|GSR-007L|FAIL|COVERAGE|", checked)
        quit(1)
        return
    print("SANSA_GODOT|GSR-007L|CASES|", case_ids.size())
    print("SANSA_GODOT|GSR-007L|POINTS|", checked)
    print("SANSA_GODOT|GSR-007L|CONTACTS|", contacts)
    print("SANSA_GODOT|GSR-007L|GRAVITY_MOVED|", gravity_moved)
    print("SANSA_GODOT|GSR-007L|CONFORMITY_MOVED|", conformity_moved)
    for field in max_errors.keys():
        print("SANSA_GODOT|GSR-007L|MAX_ERROR|", field, "|", max_errors[field])
    print("SANSA_GODOT|GSR-007L|MISMATCHES|", mismatch)
    if mismatch > 0:
        print("SANSA_GODOT|GSR-007L|FAIL")
        quit(1)
        return
    print("SANSA_GODOT|GSR-007L|PASS")
    quit(0)
