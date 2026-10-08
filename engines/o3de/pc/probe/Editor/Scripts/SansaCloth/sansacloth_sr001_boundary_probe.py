"""BF-005/006: SR-001 Flat C=0/G=0 semantic boundary (validation only)."""

PREFIX = "SANSA_O3DE|"


def run():
    try:
        import azlmbr.sansacloth_probe as probe
    except ImportError:
        print(f"{PREFIX}SR-001.DISCOVERY|CPP_GEM_NOT_LOADED")
        print(f"{PREFIX}BF-005.RESULT|OPEN")
        print(f"{PREFIX}BF-006.RESULT|OPEN")
        return False

    print(f"{PREFIX}SR-001.DISCOVERY|CPP_PROBE_AVAILABLE")
    if not probe.RunBf005006():
        print(f"{PREFIX}SR-001.PYTHON_BRIDGE_RESULT|FAIL")
        raise RuntimeError("BF-005/006 SR-001 C++ semantic boundary probe failed")
    print(f"{PREFIX}SR-001.PYTHON_BRIDGE_RESULT|PASS")
    return True


try:
    probe_result = run()
except Exception as exc:
    print(f"{PREFIX}SR-001.PYTHON_PROBE_RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
else:
    print(f"{PREFIX}SR-001.PYTHON_PROBE_RESULT|{'PASS' if probe_result else 'OPEN'}")
