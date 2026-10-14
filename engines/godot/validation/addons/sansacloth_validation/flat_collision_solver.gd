extends RefCounted

# Validation-only flat surface collision response.
# This is not the complete SansaCloth SurfaceResponse solver.
# surface_normal must be normalized; tolerance_m must be nonnegative.
func correct_point(
        point_m: Vector3,
        surface_position_m: Vector3,
        surface_normal: Vector3,
        tolerance_m: float
) -> Vector3:
    assert(absf(surface_normal.length() - 1.0) < 0.000001)
    assert(tolerance_m >= 0.0)
    var signed_separation_m := (point_m - surface_position_m).dot(surface_normal)
    if signed_separation_m >= -tolerance_m:
        return point_m
    return point_m + surface_normal * (-tolerance_m - signed_separation_m)
