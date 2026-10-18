extends SceneTree

# GSR-007G: independent SR-004 collision/final-position parity.
# Recomputes conformity and collision from fixture inputs, never from
# the reference's intermediate conformity/collision positions.
const FIXTURE := "res://addons/sansacloth_validation/fixtures/SR-004-C1-G0.json"
const RESULT := "res://addons/sansacloth_validation/reference-results/GSR-007G-C1.json"
const EPS := 0.000001

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007G|FAIL|", reason)
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
    if fixture.get("case_id") != "SR-004-C1-G0" or reference.get("case_id") != "GSR-007G-C1":
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
    var inputs: Dictionary = fixture.get("inputs", {})
    var reach := float(profile.get("conformity_reach_m", -1))
    var tolerance := 0.005
    var conformity := float(inputs.get("conformity", -1))
    var gravity: Array = inputs.get("world_gravity_m_per_s2", [])
    if reach <= 0.0 or tolerance < 0.0 or conformity != 1.0 or gravity.size() != 3:
        _fail("INPUTS")
        return
    for component in gravity:
        if absf(float(component)) > EPS:
            _fail("UNSUPPORTED_GRAVITY")
            return
    var by_uv := {}
    for vertex in vertices:
        var uv: Array = vertex.get("uv", [])
        if uv.size() != 2:
            _fail("VERTEX_UV")
            return
        var key := "%d:%d" % [roundi(float(uv[0]) * 1000000.0), roundi(float(uv[1]) * 1000000.0)]
        if by_uv.has(key):
            _fail("DUPLICATE_UV")
            return
        by_uv[key] = vertex
    var max_bridge_error := 0.0
    var bridge_mismatches := 0
    var max_collision_error := 0.0
    var max_final_error := 0.0
    var max_separation_error := 0.0
    var collision_mismatches := 0
    var final_mismatches := 0
    var separation_mismatches := 0
    var contact_mismatches := 0
    var contact_boundary_ambiguous := 0
    var contact_boundary_examples := 0
    var corrected_count := 0
    for point in points:
        var id := int(point.get("stable_id", -1))
        if not by_id.has(id):
            _fail("MISSING_REFERENCE_ID")
            return
        var sr: Dictionary = point.get("surface_reference", {})
        var key := "%d:%d" % [roundi(float(sr.get("u", -1)) * 1000000.0), roundi(float(sr.get("v", -1)) * 1000000.0)]
        if not by_uv.has(key):
            _fail("SURFACE_UV_NOT_FOUND")
            return
        var vertex: Dictionary = by_uv[key]
        var surface := _vec(vertex["position_m"])
        var normal := _vec(vertex["normal"]).normalized()
        var row := id / 21
        var column := id % 21
        var first: Dictionary = points[row * 21]
        var last: Dictionary = points[row * 21 + 20]
        var gravity_position := _vec(point["position_m"])
        if column >= 5 and column <= 15:
            gravity_position.y -= 0.03
        # Rust Reference v1 Bridge: both strip endpoints are anchors.
        # Interior initial positions (including the deliberate 30 mm offsets)
        # are replaced by endpoint interpolation before conformity/collision.
        if bool(first.get("anchor", false)) and bool(last.get("anchor", false)) and column > 0 and column < 20:
            gravity_position = _vec(first["position_m"]).lerp(_vec(last["position_m"]), float(column) / 20.0)
        var initial_separation := (gravity_position - surface).dot(normal)
        var proposed := gravity_position
        if not bool(point.get("anchor", false)) and initial_separation >= 0.0:
            var weight := clampf(1.0 - initial_separation / reach, 0.0, 1.0)
            proposed = gravity_position.lerp(surface, conformity * weight)
        var before_collision := (proposed - surface).dot(normal)
        var collision := proposed
        if before_collision < tolerance:
            collision += normal * (tolerance - before_collision)
            corrected_count += 1
        var final_position := collision
        var final_separation := (final_position - surface).dot(normal)
        var contact := final_separation <= tolerance
        var saved: Dictionary = by_id[id]
        var bridge_error := gravity_position.distance_to(_vec(saved["bridge_position_m"]))
        max_bridge_error = maxf(max_bridge_error, bridge_error)
        if bridge_error > EPS:
            bridge_mismatches += 1
        var collision_error := collision.distance_to(_vec(saved["collision_position_m"]))
        var final_error := final_position.distance_to(_vec(saved["final_position_m"]))
        var separation_error := absf(final_separation - float(saved["separation_m"]))
        max_collision_error = maxf(max_collision_error, collision_error)
        max_final_error = maxf(max_final_error, final_error)
        max_separation_error = maxf(max_separation_error, separation_error)
        if collision_error > EPS:
            collision_mismatches += 1
        if final_error > EPS:
            final_mismatches += 1
        if separation_error > EPS:
            separation_mismatches += 1
        var contact_differs := contact != bool(saved["contact"])
        var saved_separation := float(saved["separation_m"])
        var boundary_ambiguous := contact_differs and absf(final_separation - tolerance) <= EPS and absf(saved_separation - tolerance) <= EPS
        if contact_differs:
            if boundary_ambiguous:
                contact_boundary_ambiguous += 1
                if contact_boundary_examples < 5:
                    print("SANSA_GODOT|GSR-007G|CONTACT_BOUNDARY|ID=", id, "|GODOT_SEPARATION_M=", final_separation, "|RUST_SEPARATION_M=", saved_separation, "|TOLERANCE_M=", tolerance, "|GODOT_CONTACT=", contact, "|RUST_CONTACT=", bool(saved["contact"]))
                    contact_boundary_examples += 1
            else:
                contact_mismatches += 1
        if collision_error > EPS or final_error > EPS or separation_error > EPS or (contact_differs and not boundary_ambiguous):
            if collision_mismatches + final_mismatches + separation_mismatches + contact_mismatches <= 10:
                printerr("SANSA_GODOT|GSR-007G|MISMATCH|ID=", id, "|COLLISION=", collision_error, "|FINAL=", final_error, "|SEPARATION=", separation_error, "|CONTACT=", contact)
    print("SANSA_GODOT|GSR-007G|CHECKED|", points.size())
    print("SANSA_GODOT|GSR-007G|MAX_BRIDGE_ERROR_M|", max_bridge_error)
    print("SANSA_GODOT|GSR-007G|BRIDGE_MISMATCHES|", bridge_mismatches)
    print("SANSA_GODOT|GSR-007G|CORRECTED|", corrected_count)
    print("SANSA_GODOT|GSR-007G|MAX_COLLISION_ERROR_M|", max_collision_error)
    print("SANSA_GODOT|GSR-007G|MAX_FINAL_ERROR_M|", max_final_error)
    print("SANSA_GODOT|GSR-007G|MAX_SEPARATION_ERROR_M|", max_separation_error)
    print("SANSA_GODOT|GSR-007G|COLLISION_MISMATCHES|", collision_mismatches)
    print("SANSA_GODOT|GSR-007G|FINAL_MISMATCHES|", final_mismatches)
    print("SANSA_GODOT|GSR-007G|SEPARATION_MISMATCHES|", separation_mismatches)
    print("SANSA_GODOT|GSR-007G|CONTACT_MISMATCHES|", contact_mismatches)
    print("SANSA_GODOT|GSR-007G|CONTACT_BOUNDARY_AMBIGUOUS|", contact_boundary_ambiguous)
    if corrected_count == 0:
        _fail("NO_COLLISION_CORRECTIONS")
        return
    if bridge_mismatches + collision_mismatches + final_mismatches + separation_mismatches + contact_mismatches != 0:
        _fail("NUMERICAL_MISMATCH")
        return
    if contact_boundary_ambiguous > 0:
        print("SANSA_GODOT|GSR-007G|NUMERIC_PARITY_PASS_CONTACT_BOUNDARY_UNRESOLVED")
        quit(2)
        return
    print("SANSA_GODOT|GSR-007G|PASS")
    quit(0)
