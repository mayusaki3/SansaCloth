extends SceneTree

# Test-first contract for GSR-002 / GSR-003.
# Run: godot --headless --path <project> --script res://addons/sansacloth_validation/tests/gsr_flat_test.gd
# These tests are expected to FAIL until the solver adapter is implemented.
const SOLVER_PATH := "res://addons/sansacloth_validation/flat_collision_solver.gd"
const EPSILON_M := 0.000001

func _initialize() -> void:
    var failed := 0
    if not ResourceLoader.exists(SOLVER_PATH):
        printerr("SANSA_GODOT|GSR-002|OPEN|SOLVER_NOT_IMPLEMENTED")
        printerr("SANSA_GODOT|GSR-003|OPEN|SOLVER_NOT_IMPLEMENTED")
        quit(1)
        return
    var solver_script = load(SOLVER_PATH)
    if solver_script == null:
        printerr("SANSA_GODOT|TEST|FAIL|SOLVER_LOAD")
        quit(1)
        return
    var solver = solver_script.new()
    # The contract uses signed separation from the surface, a normalized normal,
    # and zero collision tolerance. It is a unit test, not full fixture parity.
    var surface := Vector3.ZERO
    var normal := Vector3.UP
    var penetrated := Vector3(0.0, -0.01, 0.0)
    var corrected: Vector3 = solver.correct_point(penetrated, surface, normal, 0.0)
    if corrected.distance_to(surface) > EPSILON_M:
        printerr("SANSA_GODOT|GSR-002|FAIL|", corrected)
        failed += 1
    else:
        print("SANSA_GODOT|GSR-002|PASS")
    var clear := Vector3(0.0, 0.01, 0.0)
    var unchanged: Vector3 = solver.correct_point(clear, surface, normal, 0.0)
    if unchanged.distance_to(clear) > EPSILON_M:
        printerr("SANSA_GODOT|GSR-003|FAIL|", unchanged)
        failed += 1
    else:
        print("SANSA_GODOT|GSR-003|PASS")
    quit(1 if failed > 0 else 0)
