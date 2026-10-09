"""OSR-006 diagnostic only: inspect all contact-boundary mismatches without changing PASS criteria."""
import json
import struct
from pathlib import Path
import azlmbr.sansacloth_probe as probe

root = Path(__file__).resolve().parent
prefix = "SANSA_O3DE|OSR-006."
def ulp_distance(a, b):
    """IEEE-754 binary64 ordered distance, for diagnosis only."""
    def ordered(value):
        bits = struct.unpack(">Q", struct.pack(">d", value))[0]
        return (~bits & 0xFFFFFFFFFFFFFFFF) if bits >> 63 else (bits | 0x8000000000000000)
    return abs(ordered(a) - ordered(b))


def stage_differences(expected, got):
    fields = ("bridge_position_m", "gravity_position_m", "conformity_position_m",
              "collision_position_m", "final_position_m", "surface_position_m",
              "surface_normal")
    for field in fields:
        for axis, (a, b) in enumerate(zip(expected[field], got[field])):
            if a != b:
                print(f"{prefix}STAGE|{field}|AXIS={axis}|REF_HEX={a.hex()}|ACT_HEX={b.hex()}|ULP={ulp_distance(a, b)}")
    query = got.get("diagnostic_query")
    if query is None:
        print(f"{prefix}QUERY_INTERMEDIATES|UNAVAILABLE|REBUILD_O3DE_GEM")
    else:
        for field, value in query.items():
            if isinstance(value, list):
                encoded = ",".join(x.hex() for x in value)
            else:
                encoded = value.hex()
            print(f"{prefix}QUERY_INTERMEDIATE|{field}|{encoded}")
    a, b = expected["separation_m"], got["separation_m"]
    print(f"{prefix}SEPARATION_HEX|REF={a.hex()}|ACT={b.hex()}|ULP={ulp_distance(a, b)}")


total = 0
failed = 0
for scenario in range(1, 6):
    for conformity in ("0", "05", "1"):
        for gravity in ("0", "1"):
            case = f"SR-{scenario:03d}-C{conformity}-G{gravity}"
            fixture = (root / "Fixtures" / f"{case}.json").read_text(encoding="utf-8")
            reference = json.loads((root / "SurfaceResponseResults" / f"{case}.json").read_text(encoding="utf-8"))
            output = probe.ProbeSurfaceResponseJson(fixture)
            if not output:
                print(f"{prefix}CASE|{case}|NO_OUTPUT")
                failed += 1
                continue
            actual = json.loads(output)
            ref_points = {p["stable_id"]: p for p in reference["control_points"]}
            got_points = {p["stable_id"]: p for p in actual["control_points"]}
            mismatches = []
            for stable_id, expected in ref_points.items():
                got = got_points.get(stable_id)
                if got is None or expected["contact"] != got["contact"]:
                    mismatches.append((stable_id, expected, got))
            total += 1
            failed += bool(mismatches)
            print(f"{prefix}CASE|{case}|CONTACT_MISMATCH_COUNT|{len(mismatches)}")
            if mismatches and mismatches[0][2] is not None:
                print(f"{prefix}STAGE_TRACE_CASE|{case}|STABLE_ID={mismatches[0][0]}")
                stage_differences(mismatches[0][1], mismatches[0][2])
            for stable_id, expected, got in mismatches[:8]:
                if got is None:
                    print(f"{prefix}POINT|{case}|{stable_id}|MISSING")
                    continue
                print(f"{prefix}POINT|{case}|{stable_id}|REF_CONTACT={expected['contact']}|ACT_CONTACT={got['contact']}|REF_SEP={expected['separation_m']:.17g}|ACT_SEP={got['separation_m']:.17g}")
                print(f"{prefix}NORMAL|{case}|{stable_id}|REF={expected['surface_normal']}|ACT={got['surface_normal']}")
                print(f"{prefix}COLLISION|{case}|{stable_id}|REF={expected['collision_position_m']}|ACT={got['collision_position_m']}")
print(f"{prefix}CHECKED|{total}")
print(f"{prefix}CASES_WITH_CONTACT_MISMATCH|{failed}")
print(f"{prefix}RESULT|DIAGNOSTIC_ONLY")
