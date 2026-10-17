extends SceneTree

# GSR-007F: collision correction matrix with independently specified expected positions.
# Reference-v1 collision does not receive anchor status; anchor penetration is corrected.
const EPS := 0.000001

func _initialize() -> void:
    var cases := [
        {"id": "penetration_zero", "p": Vector3(0.0, -0.01, 0.0), "n": Vector3.UP, "s": -0.01, "t": 0.0, "expected": Vector3.ZERO, "corrected": true},
        {"id": "penetration_tolerance", "p": Vector3(0.0, -0.01, 0.0), "n": Vector3.UP, "s": -0.01, "t": 0.005, "expected": Vector3(0.0, 0.005, 0.0), "corrected": true},
        {"id": "outside_below_tolerance", "p": Vector3(0.0, 0.002, 0.0), "n": Vector3.UP, "s": 0.002, "t": 0.005, "expected": Vector3(0.0, 0.005, 0.0), "corrected": true},
        {"id": "at_tolerance", "p": Vector3(0.0, 0.005, 0.0), "n": Vector3.UP, "s": 0.005, "t": 0.005, "expected": Vector3(0.0, 0.005, 0.0), "corrected": false},
        {"id": "outside_tolerance", "p": Vector3(0.0, 0.01, 0.0), "n": Vector3.UP, "s": 0.01, "t": 0.005, "expected": Vector3(0.0, 0.01, 0.0), "corrected": false},
        {"id": "anchor_penetration", "p": Vector3(0.0, -0.004, 0.0), "n": Vector3.UP, "s": -0.004, "t": 0.0, "expected": Vector3.ZERO, "corrected": true},
        {"id": "diagonal_unnormalized", "p": Vector3(-0.01, 0.0, 0.0), "n": Vector3(2.0, 0.0, 0.0), "s": -0.01, "t": 0.005, "expected": Vector3(0.005, 0.0, 0.0), "corrected": true},
    ]
    var corrected := 0
    var mismatches := 0
    var max_error := 0.0
    for item in cases:
        var before: Vector3 = item["p"]
        var normal: Vector3 = item["n"]
        var separation: float = item["s"]
        var tolerance: float = item["t"]
        var result := before
        if separation < tolerance:
            result += normal.normalized() * (tolerance - separation)
        var error := result.distance_to(item["expected"])
        max_error = maxf(max_error, error)
        var did_correct := result.distance_to(before) > EPS
        if did_correct:
            corrected += 1
        if error > EPS or did_correct != item["corrected"]:
            mismatches += 1
            printerr("SANSA_GODOT|GSR-007F|MISMATCH|", item["id"], "|ERROR_M|", error)
        print("SANSA_GODOT|GSR-007F|CASE|", item["id"], "|CORRECTED|", did_correct, "|ERROR_M|", error)
    print("SANSA_GODOT|GSR-007F|CHECKED|", cases.size())
    print("SANSA_GODOT|GSR-007F|CORRECTED|", corrected)
    print("SANSA_GODOT|GSR-007F|MAX_ERROR_M|", max_error)
    print("SANSA_GODOT|GSR-007F|MISMATCHES|", mismatches)
    if corrected != 5 or mismatches != 0:
        printerr("SANSA_GODOT|GSR-007F|FAIL")
        quit(1)
        return
    print("SANSA_GODOT|GSR-007F|PASS")
    quit(0)
