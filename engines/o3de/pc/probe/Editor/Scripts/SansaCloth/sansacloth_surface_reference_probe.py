"""BF-003: validation-only single-domain SurfaceReference mapping in O3DE Editor."""

PREFIX = "SANSA_O3DE|"


def run():
    try:
        import azlmbr.sansacloth_probe as probe
    except ImportError:
        print(f"{PREFIX}BF-003.DISCOVERY|CPP_GEM_NOT_LOADED")
        print(f"{PREFIX}BF-003.RESULT|OPEN")
        return

    print(f"{PREFIX}BF-003.DISCOVERY|CPP_PROBE_AVAILABLE")
    if not probe.RunBf003():
        print(f"{PREFIX}BF-003.PYTHON_BRIDGE_RESULT|FAIL")
        raise RuntimeError("BF-003 C++ SurfaceReference probe failed")
    print(f"{PREFIX}BF-003.PYTHON_BRIDGE_RESULT|PASS")


try:
    run()
except Exception as exc:
    print(f"{PREFIX}BF-003.PYTHON_PROBE_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
else:
    print(f"{PREFIX}BF-003.PYTHON_PROBE_RESULT|PASS")
