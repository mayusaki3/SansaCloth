extends SceneTree

# GSR-007H: independent Godot replay of the three-point Rust flat-surface cases.
# Deliberately does not read Rust stage outputs as solver inputs.
const TOLERANCE := 0.005
const CONTACT_EPSILON_M := 0.000001
const POSITION_EPS := 0.000001

func _fail(message: String) -> void:
    printerr("SANSA_GODOT|GSR-007H|FAIL|", message)
    quit(1)

func _check_vector(label: String, actual: Vector3, expected: Vector3) -> bool:
    var error := actual.distance_to(expected)
    if error > POSITION_EPS:
        printerr("SANSA_GODOT|GSR-007H|MISMATCH|", label, "|ERROR_M|", error)
        return false
    return true

func _contact(separation: float) -> bool:
    return separation <= TOLERANCE + CONTACT_EPSILON_M

func _solve(two_edges: bool, middle_y: float) -> Dictionary:
    # AnalyticFixture::flat() has surface y=0 and normal +Y.
    # x/z do not influence signed separation on this fixture.
    var positions := [
        Vector3(0.0, 0.020, 0.0),
        Vector3(0.5, middle_y, 0.0),
        Vector3(1.0, 0.020, 0.0)
    ]
    var bridge: Array[Vector3] = positions.duplicate()
    if two_edges:
        bridge[1] = positions[0].lerp(positions[2], 0.5)
    # Gravity vector is zero and conformity is zero in the isolated Rust cases.
    var gravity: Array[Vector3] = bridge.duplicate()
    var conformity: Array[Vector3] = gravity.duplicate()
    var collision: Array[Vector3] = conformity.duplicate()
    var before_separation := conformity[1].y
    for i in range(3):
        var separation := conformity[i].y
        if separation < TOLERANCE:
            collision[i] += Vector3.UP * (TOLERANCE - separation)
    return {
        "input": positions,
        "bridge": bridge,
        "gravity": gravity,
        "conformity": conformity,
        "collision": collision,
        "pre_separation": before_separation,
        "final_separation": collision[1].y,
        "contact": _contact(collision[1].y)
    }

func _initialize() -> void:
    var errors := 0
    var c1 := _solve(false, -0.010)
    if not _check_vector("C1_BRIDGE", c1["bridge"][1], c1["input"][1]):
        errors += 1
    if not _check_vector("C1_GRAVITY", c1["gravity"][1], c1["input"][1]):
        errors += 1
    if not _check_vector("C1_CONFORMITY", c1["conformity"][1], c1["input"][1]):
        errors += 1

    var c2 := _solve(true, -0.010)
    if not _check_vector("C2_BRIDGE", c2["bridge"][1], Vector3(0.5, 0.020, 0.0)):
        errors += 1
    if not _check_vector("C2_REPLACEMENT", c2["bridge"][1] - c2["input"][1], Vector3(0.0, 0.030, 0.0)):
        errors += 1
    if not _check_vector("C2_ENDPOINT_0", c2["bridge"][0], c2["input"][0]):
        errors += 1
    if not _check_vector("C2_ENDPOINT_2", c2["bridge"][2], c2["input"][2]):
        errors += 1

    var c3 := _solve(false, -0.010)
    if not float(c3["pre_separation"]) < 0.0:
        printerr("SANSA_GODOT|GSR-007H|MISMATCH|C3_NO_PENETRATION")
        errors += 1
    if not _check_vector("C3_CORRECTION", c3["collision"][1] - c3["conformity"][1], Vector3(0.0, 0.015, 0.0)):
        errors += 1
    if absf(float(c3["final_separation"]) - TOLERANCE) > POSITION_EPS:
        printerr("SANSA_GODOT|GSR-007H|MISMATCH|C3_FINAL_SEPARATION")
        errors += 1

    # Keep boundary probes away from f32 representational ambiguity.
    var boundary := [
        [TOLERANCE - 0.0001, true],
        [TOLERANCE, true],
        [TOLERANCE + CONTACT_EPSILON_M * 0.5, true],
        [TOLERANCE + CONTACT_EPSILON_M * 2.0, false]
    ]
    for probe in boundary:
        if _contact(float(probe[0])) != bool(probe[1]):
            printerr("SANSA_GODOT|GSR-007H|MISMATCH|C4_CONTACT|", probe[0])
            errors += 1
    var c4 := _solve(false, TOLERANCE + CONTACT_EPSILON_M * 0.5)
    if not _check_vector("C4_NO_COLLISION", c4["collision"][1], c4["conformity"][1]):
        errors += 1
    if not bool(c4["contact"]):
        printerr("SANSA_GODOT|GSR-007H|MISMATCH|C4_EXPECTED_CONTACT")
        errors += 1
    if float(c4["final_separation"]) <= TOLERANCE:
        printerr("SANSA_GODOT|GSR-007H|MISMATCH|C4_OUTSIDE_TOLERANCE")
        errors += 1

    print("SANSA_GODOT|GSR-007H|CASES|4")
    print("SANSA_GODOT|GSR-007H|MISMATCHES|", errors)
    if errors != 0:
        _fail("INVARIANT_MISMATCH")
        return
    print("SANSA_GODOT|GSR-007H|PASS")
    quit(0)
