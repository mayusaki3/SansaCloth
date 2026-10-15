extends SceneTree

# GSR-007D: independent comparison against committed SR-004 reference snapshot.
# Snapshot geometry uses incoming-edge normals; current Rust analytic normals
# differ, and generator consistency must be investigated separately.
const FIXTURE := "res://addons/sansacloth_validation/fixtures/SR-004-C1-G0.json"
const RESULT := "res://addons/sansacloth_validation/reference-results/SR-004-C1-G0.json"
const EPS := 0.000001

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007D|FAIL|", reason)
    quit(1)

func _read(path: String) -> Variant:
    if not FileAccess.file_exists(path):
        return null
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        return null
    return JSON.parse_string(file.get_as_text())

func _vec(a: Array) -> Vector3:
    return Vector3(float(a[0]), float(a[1]), float(a[2]))

func _initialize() -> void:
    var fixture = _read(FIXTURE)
    var reference = _read(RESULT)
    if not (fixture is Dictionary) or not (reference is Dictionary):
        _fail("MISSING_JSON")
        return
    if fixture.get("case_id") != "SR-004-C1-G0" or reference.get("case_id") != "SR-004-C1-G0":
        _fail("CASE_ID")
        return
    var vertices: Array = fixture.get("body_surface", {}).get("vertices", [])
    var points: Array = fixture.get("cloth", {}).get("control_points", [])
    var results: Array = reference.get("control_points", [])
    if vertices.size() != 147 or points.size() != 147 or results.size() != 147:
        _fail("COUNTS")
        return
    var by_id := {}
    for item in results:
        var id := int(item.get("stable_id", -1))
        if id < 0 or by_id.has(id):
            _fail("REFERENCE_IDS")
            return
        by_id[id] = item
    var profile: Dictionary = reference.get("profile", {})
    var reach := float(profile.get("conformity_reach_m", -1))
    if reach <= 0.0 or float(fixture.get("inputs", {}).get("conformity", -1)) != 1.0:
        _fail("INPUTS")
        return
    var max_surface_error := 0.0
    var max_normal_error := 0.0
    var max_conformity_error := 0.0
    var surface_mismatches := 0
    var conformity_mismatches := 0
    for point in points:
        var id := int(point.get("stable_id", -1))
        if not by_id.has(id):
            _fail("MISSING_REFERENCE_ID")
            return
        var ref: Dictionary = by_id[id]
        var uv_ref: Dictionary = point.get("surface_reference", {})
        var u := float(uv_ref.get("u", -1))
        var v := float(uv_ref.get("v", -1))
        var vertex: Dictionary = {}
        for candidate in vertices:
            var uv: Array = candidate.get("uv", [])
            if uv.size() == 2 and absf(float(uv[0]) - u) <= EPS and absf(float(uv[1]) - v) <= EPS:
                vertex = candidate
                break
        if vertex.is_empty():
            _fail("SURFACE_UV_NOT_FOUND")
            return
        var surface := _vec(vertex["position_m"])
        # The committed SR-004 reference snapshot uses the incoming U-edge
        # slope, rather than the analytic derivative in the current Rust source.
        # Keep this snapshot-specific rule explicit until generator parity is resolved.
        var left: Dictionary = {}
        var nearest_gap := INF
        for candidate in vertices:
            var candidate_uv: Array = candidate.get("uv", [])
            if candidate_uv.size() != 2 or absf(float(candidate_uv[1]) - v) > EPS:
                continue
            var du := u - float(candidate_uv[0])
            if du > EPS and du < nearest_gap:
                nearest_gap = du
                left = candidate
        var normal := Vector3.UP
        if not left.is_empty():
            var tangent := (surface - _vec(left["position_m"])).normalized()
            normal = Vector3(-tangent.y, tangent.x, 0.0).normalized()
        var surface_error := surface.distance_to(_vec(ref["surface_position_m"]))
        var normal_error := normal.distance_to(_vec(ref["surface_normal"]))
        max_surface_error = maxf(max_surface_error, surface_error)
        max_normal_error = maxf(max_normal_error, normal_error)
        if surface_error > EPS or normal_error > EPS:
            surface_mismatches += 1
        # Reference-v1 conformity: separation-weighted lerp toward surface.
        # This is computed from fixture geometry and inputs, not result snapshots.
        var gravity_position := _vec(point["position_m"])
        var separation := (gravity_position - surface).dot(normal)
        var proposed := gravity_position
        if not bool(point.get("anchor", false)) and separation >= 0.0:
            var weight := clampf(1.0 - separation / reach, 0.0, 1.0)
            var conformity := float(fixture["inputs"]["conformity"])
            proposed = gravity_position.lerp(surface, conformity * weight)
        var error := proposed.distance_to(_vec(ref["conformity_position_m"]))
        max_conformity_error = maxf(max_conformity_error, error)
        if error > EPS:
            conformity_mismatches += 1
            if conformity_mismatches <= 5:
                printerr("SANSA_GODOT|GSR-007D|MISMATCH|ID=", id, "|ERROR_M=", error)
    print("SANSA_GODOT|GSR-007D|CHECKED|", points.size())
    print("SANSA_GODOT|GSR-007D|MAX_SURFACE_ERROR_M|", max_surface_error)
    print("SANSA_GODOT|GSR-007D|MAX_NORMAL_ERROR|", max_normal_error)
    print("SANSA_GODOT|GSR-007D|MAX_CONFORMITY_ERROR_M|", max_conformity_error)
    print("SANSA_GODOT|GSR-007D|SURFACE_MISMATCHES|", surface_mismatches)
    print("SANSA_GODOT|GSR-007D|CONFORMITY_MISMATCHES|", conformity_mismatches)
    if surface_mismatches != 0 or conformity_mismatches != 0:
        _fail("NUMERICAL_MISMATCH")
        return
    print("SANSA_GODOT|GSR-007D|PASS")
    quit(0)
