"""OSR validation-only comparator. Never treats a Fixture ACK as solver output."""
import math

FORMAT = "sansacloth.validation.surface-response-result/0"
VECTORS = (
    "bridge_position_m", "gravity_position_m", "conformity_position_m",
    "collision_position_m", "final_position_m", "surface_position_m",
    "surface_normal",
)
SCALARS = ("separation_m",)
FIELDS = ("stable_id", "support", "contact") + VECTORS + SCALARS


def _number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name}: expected number")
    number = float(value)
    if not math.isfinite(number):
        raise ValueError(f"{name}: nonfinite")
    return number


def _points(document):
    if not isinstance(document, dict) or document.get("format") != FORMAT:
        raise ValueError("unsupported SurfaceResponse format")
    if not isinstance(document.get("case_id"), str) or not document["case_id"]:
        raise ValueError("missing case_id")
    points = document.get("control_points")
    if not isinstance(points, list) or len(points) != 147:
        raise ValueError("expected 147 control points")
    indexed = {}
    for point in points:
        if not isinstance(point, dict) or any(field not in point for field in FIELDS):
            raise ValueError("incomplete control point")
        stable_id = point["stable_id"]
        if isinstance(stable_id, bool) or not isinstance(stable_id, int) or stable_id < 0:
            raise ValueError("invalid stable_id")
        if stable_id in indexed:
            raise ValueError(f"duplicate stable_id {stable_id}")
        if point["support"] not in ("Anchor", "Unsupported") or type(point["contact"]) is not bool:
            raise ValueError(f"invalid support/contact {stable_id}")
        for field in VECTORS:
            vector = point[field]
            if not isinstance(vector, list) or len(vector) != 3:
                raise ValueError(f"{field}: expected vec3")
            for axis, value in enumerate(vector):
                _number(value, f"{stable_id}.{field}[{axis}]")
        for field in SCALARS:
            _number(point[field], f"{stable_id}.{field}")
        indexed[stable_id] = point
    if set(indexed) != set(range(147)):
        raise ValueError("stable_id set must be 0..146")
    aggregate = document.get("aggregate")
    if not isinstance(aggregate, dict):
        raise ValueError("missing aggregate")
    expected = {
        "control_point_count": len(indexed),
        "support_count": sum(p["support"] != "Unsupported" for p in indexed.values()),
        "contact_count": sum(p["contact"] for p in indexed.values()),
    }
    for key, value in expected.items():
        if type(aggregate.get(key)) is not int or aggregate[key] != value:
            raise ValueError(f"aggregate mismatch: {key}")
    return indexed


def compare(reference, actual, *, absolute_tolerance_m, normal_tolerance):
    """Compare independently computed canonical/m SurfaceResponse documents.

    Returns per-field maximum absolute differences. Raises on any mismatch.
    The caller must obtain actual from an independently executed backend solver.
    """
    tolerance = _number(absolute_tolerance_m, "absolute_tolerance_m")
    normal_limit = _number(normal_tolerance, "normal_tolerance")
    if tolerance < 0 or normal_limit < 0:
        raise ValueError("negative tolerance")
    ref = _points(reference)
    got = _points(actual)
    if reference["case_id"] != actual["case_id"]:
        raise ValueError("case_id mismatch")
    if reference.get("profile") != actual.get("profile"):
        raise ValueError("profile mismatch")
    maxima = {field: 0.0 for field in VECTORS + SCALARS}
    for stable_id in sorted(ref):
        expected, observed = ref[stable_id], got[stable_id]
        for field in ("support", "contact"):
            if expected[field] != observed[field]:
                raise ValueError(f"{stable_id}.{field}: mismatch")
        for field in VECTORS:
            delta = max(abs(_number(a, field) - _number(b, field))
                        for a, b in zip(expected[field], observed[field]))
            maxima[field] = max(maxima[field], delta)
            limit = normal_limit if field == "surface_normal" else tolerance
            if delta > limit:
                raise ValueError(f"{stable_id}.{field}: error {delta} exceeds {limit}")
        for field in SCALARS:
            delta = abs(_number(expected[field], field) - _number(observed[field], field))
            maxima[field] = max(maxima[field], delta)
            if delta > tolerance:
                raise ValueError(f"{stable_id}.{field}: error {delta} exceeds {tolerance}")
    return maxima
