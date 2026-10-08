"""BF-004: validation-only rigid single-domain SurfaceQuery in O3DE Editor."""

PREFIX = "SANSA_O3DE|"


def run():
    try:
        import azlmbr.sansacloth_probe as probe
    except ImportError:
        print(f"{PREFIX}BF-004.DISCOVERY|CPP_GEM_NOT_LOADED")
        print(f"{PREFIX}BF-004.RESULT|OPEN")
        return False

    print(f"{PREFIX}BF-004.DISCOVERY|CPP_PROBE_AVAILABLE")
    if not probe.RunBf004():
        print(f"{PREFIX}BF-004.PYTHON_BRIDGE_RESULT|FAIL")
        raise RuntimeError("BF-004 C++ SurfaceQuery probe failed")
    print(f"{PREFIX}BF-004.PYTHON_BRIDGE_RESULT|PASS")
    return True


try:
    probe_result = run()
except Exception as exc:
    print(f"{PREFIX}BF-004.PYTHON_PROBE_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
else:
    print(f"{PREFIX}BF-004.PYTHON_PROBE_RESULT|{'PASS' if probe_result else 'OPEN'}")
