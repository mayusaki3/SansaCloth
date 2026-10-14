extends SceneTree

# GSR-007D prerequisite: ensure chosen reference fixture exercises conformity.
# Reference inspection only. This is NOT independent solver parity.
const FIXTURE := "res://addons/sansacloth_validation/fixtures/SR-004-C1-G0.json"
const RESULT := "res://addons/sansacloth_validation/reference-results/SR-004-C1-G0.json"
const EPS := 0.00000001

func _fail(reason: String) -> void:
    printerr("SANSA_GODOT|GSR-007D-PRE|FAIL|", reason)
    quit(1)

func _read(path: String) -> Variant:
    if not FileAccess.file_exists(path):
        return null
    var f := FileAccess.open(path, FileAccess.READ)
    if f == null:
        return null
    return JSON.parse_string(f.get_as_text())

func _initialize() -> void:
    var fixture = _read(FIXTURE)
    var result = _read(RESULT)
    if not (fixture is Dictionary) or not (result is Dictionary):
        _fail("MISSING_OR_INVALID_JSON")
        return
    if fixture.get("case_id") != "SR-004-C1-G0" or result.get("case_id") != "SR-004-C1-G0":
        _fail("CASE_ID")
        return
    var inputs: Dictionary = fixture.get("inputs", {})
    if float(inputs.get("conformity", -1)) != 1.0:
        _fail("CONFORMITY_INPUT")
        return
    var gravity: Array = inputs.get("world_gravity_m_per_s2", [])
    if gravity.size() != 3:
        _fail("GRAVITY_SCHEMA")
        return
    for component in gravity:
        if absf(float(component)) > EPS:
            _fail("NONZERO_GRAVITY")
            return
    var points: Array = result.get("control_points", [])
    if points.size() != 147:
        _fail("COUNT")
        return
    var changed := 0
    var max_delta := 0.0
    for point in points:
        var a: Array = point.get("gravity_position_m", [])
        var b: Array = point.get("conformity_position_m", [])
        if a.size() != 3 or b.size() != 3:
            _fail("POSITION_SCHEMA")
            return
        var delta := Vector3(float(a[0])-float(b[0]), float(a[1])-float(b[1]), float(a[2])-float(b[2])).length()
        max_delta = maxf(max_delta, delta)
        if delta > EPS:
            changed += 1
    print("SANSA_GODOT|GSR-007D-PRE|CHECKED|", points.size())
    print("SANSA_GODOT|GSR-007D-PRE|CHANGED|", changed)
    print("SANSA_GODOT|GSR-007D-PRE|MAX_DELTA_M|", max_delta)
    if changed != 63:
        _fail("UNEXPECTED_CHANGED_COUNT")
        return
    print("SANSA_GODOT|GSR-007D-PRE|PASS")
    quit(0)
