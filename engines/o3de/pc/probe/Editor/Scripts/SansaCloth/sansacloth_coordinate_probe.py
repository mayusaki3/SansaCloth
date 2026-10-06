import math as pymath

import azlmbr.math as azmath


PREFIX = "SANSA_O3DE|"


def log(key, value):
    print(f"{PREFIX}{key}|{value}")


def vec(v):
    return f"{v.GetX():.9g},{v.GetY():.9g},{v.GetZ():.9g}"


def canonical_to_o3de(v):
    # Candidate mapping under validation:
    # canonical +X=right,+Y=up,+Z=forward
    # O3DE      +X=right,+Y=forward,+Z=up
    return azmath.Vector3(v.GetX(), v.GetZ(), v.GetY())


def rotation_transform(axis, radians):
    factory = getattr(azmath, f"Quaternion_CreateRotation{axis}")
    quat = factory(radians)
    return azmath.Transform_CreateFromQuaternionAndTranslation(
        quat, azmath.Vector3(0.0, 0.0, 0.0)
    )


def rotate(axis, degrees, v):
    transform = rotation_transform(axis, pymath.radians(degrees))
    return transform.TransformPoint(v)


def try_cross(lhs, rhs):
    cross_method = getattr(lhs, "Cross", None)
    if callable(cross_method):
        return "Vector3.Cross", cross_method(rhs)

    cross_function = getattr(azmath, "Vector3_Cross", None)
    if callable(cross_function):
        return "Vector3_Cross", cross_function(lhs, rhs)

    return None, None


def run():
    x = azmath.Vector3(1.0, 0.0, 0.0)
    y = azmath.Vector3(0.0, 1.0, 0.0)
    z = azmath.Vector3(0.0, 0.0, 1.0)

    log("OBF-001.WORLD_UNIT_M", "1")
    log("OBF-001.CLOTH_WIDTH_M", "0.2")
    log("OBF-001.RESULT", "OBSERVED")

    log("OBF-002.O3DE_BASIS_X", vec(x))
    log("OBF-002.O3DE_BASIS_Y", vec(y))
    log("OBF-002.O3DE_BASIS_Z", vec(z))
    log("OBF-002.CANONICAL_X_TO_O3DE", vec(canonical_to_o3de(x)))
    log("OBF-002.CANONICAL_Y_TO_O3DE", vec(canonical_to_o3de(y)))
    log("OBF-002.CANONICAL_Z_TO_O3DE", vec(canonical_to_o3de(z)))
    log("OBF-002.RESULT", "OBSERVED")

    for axis in ("X", "Y", "Z"):
        for degrees in (90, -90):
            log(
                f"OBF-003.ROT_{axis}_{degrees:+d}_X",
                vec(rotate(axis, degrees, x)),
            )
            log(
                f"OBF-003.ROT_{axis}_{degrees:+d}_Y",
                vec(rotate(axis, degrees, y)),
            )
            log(
                f"OBF-003.ROT_{axis}_{degrees:+d}_Z",
                vec(rotate(axis, degrees, z)),
            )

    # Canonical +90deg around +Z maps +X to +Y.
    # Under the candidate axis permutation this should map O3DE +X to +Z.
    log("OBF-003.CANONICAL_Z_PLUS90_EXPECTED_O3DE", vec(z))
    log("OBF-003.O3DE_Y_PLUS90_APPLIED_X", vec(rotate("Y", 90, x)))
    log("OBF-003.O3DE_Y_MINUS90_APPLIED_X", vec(rotate("Y", -90, x)))
    log("OBF-003.RESULT", "OBSERVED")

    cross_api, xy = try_cross(x, y)
    if cross_api is None:
        log("OBF-004.API", "NOT_EXPOSED")
        log("OBF-004.RESULT", "OPEN")
    else:
        _, yz = try_cross(y, z)
        _, zx = try_cross(z, x)
        log("OBF-004.API", cross_api)
        log("OBF-004.X_CROSS_Y", vec(xy))
        log("OBF-004.Y_CROSS_Z", vec(yz))
        log("OBF-004.Z_CROSS_X", vec(zx))
        log("OBF-004.RESULT", "OBSERVED")

    log("OBF-01.PYTHON_PROBE_RESULT", "PASS")


try:
    run()
except Exception as exc:
    log("OBF-01.PYTHON_PROBE_RESULT", f"FAIL|{type(exc).__name__}: {exc}")
    raise
