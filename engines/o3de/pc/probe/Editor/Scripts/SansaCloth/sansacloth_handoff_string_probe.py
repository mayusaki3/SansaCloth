"""OXC-001 validation-only BehaviorContext string argument/return spike."""
PREFIX = "SANSA_O3DE|"
try:
    import azlmbr.sansacloth_probe as probe
except ImportError as exc:
    print(f"{PREFIX}OXC-001.DISCOVERY|CPP_GEM_NOT_LOADED")
    print(f"{PREFIX}OXC-001.RESULT|OPEN")
    raise

try:
    fn = getattr(probe, "ProbeHandoffString")
    print(f"{PREFIX}OXC-001.DISCOVERY|CPP_STRING_HANDOFF_AVAILABLE")
    received = fn("SANSA-HANDOFF-V0|probe")
    if received != "SANSA-HANDOFF-V0|ACK":
        raise ValueError(f"unexpected C++ ACK: {received!r}")
    rejected = fn("SANSA-HANDOFF-V0|invalid")
    if rejected != "":
        raise ValueError(f"invalid payload accepted: {rejected!r}")
    print(f"{PREFIX}OXC-001.ROUNDTRIP|PASS")
    print(f"{PREFIX}OXC-001.INVALID_REJECTED|PASS")
    print(f"{PREFIX}OXC-001.RESULT|PASS")
except Exception as exc:
    print(f"{PREFIX}OXC-001.RESULT|FAIL|{type(exc).__name__}: {exc}")
    raise
