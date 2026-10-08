import math as pymath

import azlmbr.math as azmath


PREFIX = "SANSA_O3DE|"


def log(key, value):
    print(f"{PREFIX}{key}|{value}")


def vec(v):
    return f"{v.x:.9g},{v.y:.9g},{v.z:.9g}"


def canonical_to_o3de(v):
    # Candidate mapping under validation:
    # canonical +X=right,+Y=up,+Z=forward
    # O3DE      +X=right,+Y=forward,+Z=up
    return azmath.Vector3(v.x, v.z, v.y)


def rotation_transform(axis, radians):
    factory = getattr(azmath, f"Quaternion_CreateRotation{axis}")
    quat = factory(radians)
    return azmath.Transform_CreateFromQuaternionAndTranslation(
        quat, azmath.Vector3(0.0, 0.0, 0.0)
    )


def rotation_bases(axis, degrees):
    transform = rotation_transform(axis, pymath.radians(degrees))
    return (
        transform.GetBasisX(),
        transform.GetBasisY(),
        transform.GetBasisZ(),
    )


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

    observed_rotations = {}
    for axis in ("X", "Y", "Z"):
        for degrees in (90, -90):
            basis_x, basis_y, basis_z = rotation_bases(axis, degrees)
            observed_rotations[(axis, degrees)] = (
                basis_x,
                basis_y,
                basis_z,
            )
            log(f"OBF-003.ROT_{axis}_{degrees:+d}_BASIS_X", vec(basis_x))
            log(f"OBF-003.ROT_{axis}_{degrees:+d}_BASIS_Y", vec(basis_y))
            log(f"OBF-003.ROT_{axis}_{degrees:+d}_BASIS_Z", vec(basis_z))

    # Canonical +90deg around +Z maps +X to +Y.
    # Under the candidate axis permutation this should map O3DE +X to +Z.
    log("OBF-003.CANONICAL_Z_PLUS90_EXPECTED_O3DE", vec(z))
    log(
        "OBF-003.O3DE_Y_PLUS90_BASIS_X",
        vec(observed_rotations[("Y", 90)][0]),
    )
    log(
        "OBF-003.O3DE_Y_MINUS90_BASIS_X",
        vec(observed_rotations[("Y", -90)][0]),
    )
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

    # OBF-005: a canonical triangle with +Y normal.
    # p0=(0,0,0), p1=(1,0,0), p2=(0,0,-1):
    # (p1-p0) cross (p2-p0) = +Y in canonical space.
    canonical_p0 = azmath.Vector3(0.0, 0.0, 0.0)
    canonical_p1 = azmath.Vector3(1.0, 0.0, 0.0)
    canonical_p2 = azmath.Vector3(0.0, 0.0, -1.0)
    canonical_normal = azmath.Vector3(0.0, 1.0, 0.0)

    o3de_p0 = canonical_to_o3de(canonical_p0)
    o3de_p1 = canonical_to_o3de(canonical_p1)
    o3de_p2 = canonical_to_o3de(canonical_p2)
    expected_o3de_normal = canonical_to_o3de(canonical_normal)

    same_edge_1 = azmath.Vector3(
        o3de_p1.x - o3de_p0.x,
        o3de_p1.y - o3de_p0.y,
        o3de_p1.z - o3de_p0.z,
    )
    same_edge_2 = azmath.Vector3(
        o3de_p2.x - o3de_p0.x,
        o3de_p2.y - o3de_p0.y,
        o3de_p2.z - o3de_p0.z,
    )
    same_winding_normal = same_edge_1.Cross(same_edge_2)
    reversed_winding_normal = same_edge_2.Cross(same_edge_1)

    log("OBF-005.EXPECTED_MAPPED_NORMAL", vec(expected_o3de_normal))
    log("OBF-005.SAME_WINDING_NORMAL", vec(same_winding_normal))
    log("OBF-005.REVERSED_WINDING_NORMAL", vec(reversed_winding_normal))

    tolerance = 1.0e-6
    reversed_matches = (
        abs(reversed_winding_normal.x - expected_o3de_normal.x) <= tolerance
        and abs(reversed_winding_normal.y - expected_o3de_normal.y) <= tolerance
        and abs(reversed_winding_normal.z - expected_o3de_normal.z) <= tolerance
    )
    same_matches = (
        abs(same_winding_normal.x - expected_o3de_normal.x) <= tolerance
        and abs(same_winding_normal.y - expected_o3de_normal.y) <= tolerance
        and abs(same_winding_normal.z - expected_o3de_normal.z) <= tolerance
    )
    log("OBF-005.SAME_WINDING_MATCH", str(same_matches).upper())
    log("OBF-005.REVERSED_WINDING_MATCH", str(reversed_matches).upper())
    if same_matches or not reversed_matches:
        log("OBF-005.RESULT", "FAIL")
        raise RuntimeError("triangle winding conversion did not match expected normal")
    log("OBF-005.RESULT", "PASS")

    # OBF-006 uses explicitly supplied world gravity and O3DE Transform.
    # It does not require a physics engine or physics scene.
    try:
        import azlmbr.sansacloth_probe as sansacloth_probe
    except ImportError:
        log("OBF-006.DISCOVERY", "CPP_GEM_NOT_LOADED")
        log("OBF-006.RESULT", "OPEN")
    else:
        log("OBF-006.DISCOVERY", "CPP_PROBE_AVAILABLE")
        cpp_result = sansacloth_probe.RunObf006()
        if not cpp_result:
            log("OBF-006.PYTHON_BRIDGE_RESULT", "FAIL")
            raise RuntimeError("OBF-006 C++ gravity probe failed")
        log("OBF-006.PYTHON_BRIDGE_RESULT", "PASS")

    log("OBF-01.PYTHON_PROBE_RESULT", "PASS")


try:
    run()
except Exception as exc:
    log("OBF-01.PYTHON_PROBE_RESULT", f"FAIL|{type(exc).__name__}: {exc}")
    raise
