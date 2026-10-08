# SansaCloth O3DE PC Probe

Validation-only O3DE PC probe set.

The O3DE validation project is a disposable runtime environment. The canonical probe source remains in this repository.

## Prerequisite

Enable the O3DE **Python Editor Bindings** Gem in the validation project and rebuild/reopen the Editor if required.

## Deploy

From the SansaCloth repository root:

~~~powershell
.\engines\o3de\pc\probe\BuildProbeAssets.ps1 -O3DEProjectDirectory "D:\path\to\O3DEProject"
~~~

Expected:

~~~text
SANSA_O3DE_BUILD|SCRIPT_COUNT|7
SANSA_O3DE_BUILD|DEPLOY_TARGET|...\Editor\Scripts\SansaCloth
SANSA_O3DE_BUILD|GEM_FILE_COUNT|6
SANSA_O3DE_BUILD|GEM_DEPLOY_TARGET|...\Gems\SansaClothBackendProbeValidation
SANSA_O3DE_BUILD|RESULT|PASS
~~~

The deploy replaces only:

~~~text
<O3DE project>\Editor\Scripts\SansaCloth
<O3DE project>\Gems\SansaClothBackendProbeValidation
~~~

## Enable the OBF-006 validation Gem

OBF-001 through OBF-005 run from Python only. OBF-006 uses a validation-only C++ Gem to exercise O3DE Transform math with explicitly supplied world-space gravity. It does not require PhysX5, a Physics Scene, or a rigid body.

After deploy, enable:

~~~text
SansaClothBackendProbeValidation
~~~

for the validation project and rebuild the project before reopening the Editor.

Using the O3DE CLI from the engine root, the enable operation is equivalent to:

~~~powershell
scripts\o3de.bat enable-gem `
    -gp "D:\path\to\O3DEProject\Gems\SansaClothBackendProbeValidation" `
    -pp "D:\path\to\O3DEProject"
~~~

The Project Manager Gem configuration UI may be used instead. This is a code Gem, so enabling it requires a project rebuild before the Python bridge can import `azlmbr.sansacloth_probe`.

## Run OBF-01

In O3DE Editor open:

~~~text
Tools > Other > Python Scripts
~~~

Run:

~~~text
SansaCloth/sansacloth_coordinate_probe.py
~~~

If the script browser does not expose the nested script, use **Tools > Other > Python Console** and execute the file through the Editor Python runner or place the script directly in the project Editor/Scripts browser as appropriate for the installed O3DE version.

Capture every line beginning with:

~~~text
SANSA_O3DE|
~~~

Do not mark BF-001/BF-002 PASS from documentation alone. Runtime output is required.

OBF-004 remains OPEN when the Python binding does not expose AZ::Vector3 Cross directly. In that case a C++ probe is added rather than reproducing the cross product in Python.


## OBF-006 result

When the validation Gem is not loaded, the Python probe reports:

~~~text
SANSA_O3DE|OBF-006.DISCOVERY|CPP_GEM_NOT_LOADED
SANSA_O3DE|OBF-006.RESULT|OPEN
~~~

When the Gem is loaded, the Python probe calls the C++ OBF-006 probe. It accepts explicit canonical world-space gravity, maps it to O3DE world-space, transforms it into body-local space with O3DE Transform math, and reconstructs the original world-space vector. Rotating the body must change the local representation but must not rotate the external world-space input.

Cases:
- C01: arbitrary gravity, identity transform
- C02: arbitrary gravity, O3DE Y +90 degrees
- C03: vertical gravity, O3DE X -90 degrees
- C04: zero gravity, O3DE Y +90 degrees

A successful run includes:

~~~text
SANSA_O3DE|OBF-006.DISCOVERY|CPP_PROBE_AVAILABLE
SANSA_O3DE|OBF-006.C01.WORLD_GRAVITY_INPUT|...
SANSA_O3DE|OBF-006.C01.BODY_LOCAL_GRAVITY|...
SANSA_O3DE|OBF-006.C01.RECONSTRUCTED_WORLD_GRAVITY|...
SANSA_O3DE|OBF-006.C01.RESULT|PASS
SANSA_O3DE|OBF-006.C02.RESULT|PASS
SANSA_O3DE|OBF-006.C03.RESULT|PASS
SANSA_O3DE|OBF-006.C04.RESULT|PASS
SANSA_O3DE|OBF-006.RESULT|PASS
SANSA_O3DE|OBF-006.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|OBF-01.PYTHON_PROBE_RESULT|PASS
~~~

This validates the coordinate/semantic boundary, not a complete SansaCloth production Runtime Backend.

## Run BF-003 SurfaceReference Mapping

After deploying and rebuilding the validation Gem, in O3DE Editor open:

~~~text
Tools > Other > Python Scripts
~~~

Run:

~~~text
SansaCloth/sansacloth_surface_reference_probe.py
~~~

The separate script calls `azlmbr.sansacloth_probe.RunBf003()`.
This probe uses a validation-only flat mesh represented by O3DE math arrays;
it does **not** access the O3DE Mesh API or claim production skinned/deformed
mesh mapping.

Expected values (O3DE coordinate space):

~~~text
SANSA_O3DE|BF-003.DISCOVERY|CPP_PROBE_AVAILABLE
SANSA_O3DE|BF-003.DOMAIN_ID|1
SANSA_O3DE|BF-003.SURFACE_REFERENCE_UV|0.25,0.75
SANSA_O3DE|BF-003.RESOLVED_TRIANGLE|0
SANSA_O3DE|BF-003.REORDERED_RESOLVED_TRIANGLE|1
SANSA_O3DE|BF-003.IDENTITY_SURFACE_POSITION|-0.05,0.025,0
SANSA_O3DE|BF-003.TRANSFORMED_SURFACE_POSITION|0.3,-0.075,0.25
SANSA_O3DE|BF-003.REORDERED_SURFACE_POSITION|-0.05,0.025,0
SANSA_O3DE|BF-003.IDENTITY_RESULT|PASS
SANSA_O3DE|BF-003.TRANSFORM_RESULT|PASS
SANSA_O3DE|BF-003.TRIANGLE_REORDER_RESULT|PASS
SANSA_O3DE|BF-003.INVALID_DOMAIN_REJECTED|TRUE
SANSA_O3DE|BF-003.INVALID_UV_REJECTED|TRUE
SANSA_O3DE|BF-003.RESULT|PASS
SANSA_O3DE|BF-003.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|BF-003.PYTHON_PROBE_RESULT|PASS
~~~

Float rounding differences in vector values are acceptable within 1e-5m.
On the first compile or runtime error, stop and capture the first error.
BF-003 was confirmed PASS in O3DE Editor on 2026-10-08 for the validation-only fixture.

## Run BF-004 SurfaceQuery Feasibility

After deploying and rebuilding the C++ validation Gem, run this script in
O3DE Editor using Tools > Other > Python Scripts:

~~~text
SansaCloth/sansacloth_surface_query_probe.py
~~~

The script calls `azlmbr.sansacloth_probe.RunBf004()`. The C++ probe
resolves DomainId=1 and UV=(0.25,0.75) on the same validation-only flat
fixture used by BF-003. It calculates triangle geometric normal from
O3DE Vector3.Cross and signed separation from the current world-space
position. No physics Gem or PhysX dependency is required.

Expected runtime log:

~~~text
SANSA_O3DE|BF-004.DISCOVERY|CPP_PROBE_AVAILABLE
SANSA_O3DE|BF-004.IDENTITY_SURFACE_POSITION|-0.05,0.025,0
SANSA_O3DE|BF-004.IDENTITY_SURFACE_NORMAL|0,0,1
SANSA_O3DE|BF-004.IDENTITY_OUTWARD_SEPARATION_M|0.01
SANSA_O3DE|BF-004.IDENTITY_INWARD_SEPARATION_M|-0.01
SANSA_O3DE|BF-004.TRANSFORMED_SURFACE_POSITION|0.3,-0.075,0.25
SANSA_O3DE|BF-004.TRANSFORMED_SURFACE_NORMAL|1,0,0
SANSA_O3DE|BF-004.TRANSFORMED_OUTWARD_SEPARATION_M|0.01
SANSA_O3DE|BF-004.TRANSFORMED_INWARD_SEPARATION_M|-0.01
SANSA_O3DE|BF-004.IDENTITY_TANGENT_SEPARATION_M|0
SANSA_O3DE|BF-004.SQF-001.RESULT|PASS
SANSA_O3DE|BF-004.SQF-002.RESULT|PASS
SANSA_O3DE|BF-004.SQF-003.RESULT|PASS
SANSA_O3DE|BF-004.SQF-004.RESULT|PASS
SANSA_O3DE|BF-004.SQF-005.RESULT|PASS
SANSA_O3DE|BF-004.SQF-006.RESULT|PASS
SANSA_O3DE|BF-004.SQF-007.RESULT|PASS
SANSA_O3DE|BF-004.RESULT|PASS
SANSA_O3DE|BF-004.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|BF-004.PYTHON_PROBE_RESULT|PASS
~~~

Float roundoff is allowed within 1e-5. Stop on the first build or runtime
error. BF-004 was confirmed PASS in O3DE Editor on 2026-10-08 for the validation-only fixture.
This does not validate production Mesh API, deforming surfaces, non-uniform
scale, or a full cloth solver.

## Run BF-005 / BF-006 SR-001 Input/Output Semantics

Deploy the current 6 Python scripts and C++ Validation Gem, rebuild the project,
and run in O3DE Editor (Tools > Other > Python Scripts):

~~~text
SansaCloth/sansacloth_sr001_boundary_probe.py
~~~

The Python script calls `azlmbr.sansacloth_probe.RunBf005006()`.

The C++ probe constructs 147 SR-001 Flat CPs (21 U × 7 V) with stable IDs
0–146, DomainId=1 and normalized UVs. Both U edges are anchors (14 CPs).
All 147 CPs have Contact input, but the 133 non-anchors must remain
Unsupported. World Gravity and Conformity are explicitly zero.
It maps the final position, logical surface reference, geometric normal,
signed separation and support to a validation-only result.

The probe reuses the BF-003/004 O3DE math-only analytic surface. C=0/G=0
is an identity stage, **not** an implementation of the SansaCloth solver.
Nonzero Gravity or Conformity is deliberately rejected.

Expected aggregate:

~~~text
SANSA_O3DE|SR-001.DISCOVERY|CPP_PROBE_AVAILABLE
SANSA_O3DE|SR-001.CP_COUNT|147
SANSA_O3DE|SR-001.ANCHOR_COUNT|14
SANSA_O3DE|SR-001.CONTACT_INPUT_COUNT|147
SANSA_O3DE|SR-001.DIRECT_SUPPORT_COUNT|14
SANSA_O3DE|SR-001.CONTACT_ONLY_UNSUPPORTED_COUNT|133
SANSA_O3DE|SR-001.OUTPUT_CP_COUNT|147
SANSA_O3DE|SR-001.DERIVED_CONTACT_COUNT|147
SANSA_O3DE|SR-001.WORLD_GRAVITY_INPUT|0,0,0
SANSA_O3DE|SR-001.CONFORMITY_INPUT|0
SANSA_O3DE|SR-001.COLLISION_TOLERANCE_M|0
SANSA_O3DE|SR-001.FINAL_POSITION_DEVIATION_MAX_M|0
SANSA_O3DE|SR-001.NORMAL_DEVIATION_MAX|0
SANSA_O3DE|SR-001.SEPARATION_ABS_MAX_M|0
SANSA_O3DE|SR-001.NONZERO_GRAVITY_REJECTED|TRUE
SANSA_O3DE|SR-001.NONZERO_CONFORMITY_REJECTED|TRUE
~~~

Representative CP output (O3DE coordinates):

~~~text
SANSA_O3DE|SR-001.CP_FIRST|id=0;domain=1;uv=0,0;position=-0.1,-0.05,0;normal=0,0,1;separation_m=0;support=Anchor
SANSA_O3DE|SR-001.CP_CENTER|id=73;domain=1;uv=0.5,0.5;position=0,0,0;normal=0,0,1;separation_m=0;support=Unsupported
SANSA_O3DE|SR-001.CP_LAST|id=146;domain=1;uv=1,1;position=0.1,0.05,0;normal=0,0,1;separation_m=0;support=Anchor
~~~

Each `SANSA_O3DE|BF-005.ISF-001.RESULT` through `ISF-007.RESULT`
and `SANSA_O3DE|BF-006.OSF-001.RESULT` through `OSF-010.RESULT`
must be `PASS`, followed by:

~~~text
SANSA_O3DE|BF-005.RESULT|PASS
SANSA_O3DE|BF-006.RESULT|PASS
SANSA_O3DE|SR-001.RESULT|PASS
SANSA_O3DE|SR-001.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|SR-001.PYTHON_PROBE_RESULT|PASS
~~~

Tolerance for positions, normals and separation is 1e-5. Stop on the
first build or runtime error. BF-005/006 were confirmed PASS in O3DE Editor on 2026-10-08 for the
validation-only SR-001 Flat C=0/G=0 fixture.

## Run BF-007 Basic Fixture Mapping

After closing O3DE Editor, deploy the current 6 Python scripts and validation Gem
using `BuildProbeAssets.ps1`. Rebuild the O3DE project in Project Manager.
In Editor > Tools > Other > Python Scripts, run:

~~~text
SansaCloth/sansacloth_basic_fixture_probe.py
~~~

The script calls `azlmbr.sansacloth_probe.RunBf007()`.
Five validation-only analytic grids are generated using the same raised
cosine equations as Unity's BF-007 probe. Each has 147 vertices, 147 CP,
240 triangles. The O3DE Y/Z axis swap requires reversed triangle winding;
SR-003 Convex-Side rotates +90 degrees around O3DE Y.

Expected fixture summaries:

| Fixture | Center position (O3DE m) | Normal | Anchor | Local height min/max (m) |
|---|---|---|---|---|
| SR-001.FLAT | 0,0,0 | 0,0,1 | 14 | 0/0 |
| SR-002.CONVEX_UP | 0,0,0.03 | 0,0,1 | 14 | 0/0.03 |
| SR-003.CONVEX_SIDE | 0.03,0,0 | 1,0,0 | 7 | 0/0.03 |
| SR-004.CONCAVE_SHALLOW | 0,0,-0.02 | 0,0,1 | 14 | -0.02/0 |
| SR-005.CONCAVE_DEEP | 0,0,-0.05 | 0,0,1 | 14 | -0.05/0 |

For every fixture, the following checks must all be PASS:
`COUNTS`, `ANCHORS`, `HEIGHT_RANGE`, `CENTER_POSITION`,
`CENTER_NORMAL`, `FIRST_TRIANGLE_NORMAL`, and
`ALL_TRIANGLE_GEOMETRY` (all 240 triangles nondegenerate and outward).
Final expected log:

~~~text
SANSA_O3DE|BF-007.DISCOVERY|CPP_PROBE_AVAILABLE
SANSA_O3DE|BF-007.SR-001.FLAT.RESULT|PASS
SANSA_O3DE|BF-007.SR-002.CONVEX_UP.RESULT|PASS
SANSA_O3DE|BF-007.SR-003.CONVEX_SIDE.RESULT|PASS
SANSA_O3DE|BF-007.SR-004.CONCAVE_SHALLOW.RESULT|PASS
SANSA_O3DE|BF-007.SR-005.CONCAVE_DEEP.RESULT|PASS
SANSA_O3DE|BF-007.RESULT|PASS
SANSA_O3DE|BF-007.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|BF-007.PYTHON_PROBE_RESULT|PASS
~~~

Stop on the first build or runtime error. BF-007 was confirmed PASS in
O3DE Editor on 2026-10-08 for all five validation-only analytic fixtures
(35 fixture checks, five per-fixture results, C++ bridge and Python probe).
The earlier CPP_GEM_NOT_LOADED state is no longer present; its precise
cause was not established. This does not validate actual O3DE
Mesh API or production mesh deformation.

## Run BF-008 Validation Capture

After closing O3DE Editor, deploy the 6 Python scripts and C++ Gem with
`BuildProbeAssets.ps1`, rebuild the project, and run this Editor script:

~~~text
SansaCloth/sansacloth_validation_capture_probe.py
~~~

The C++ `RunBf008Capture()` method uses the BF-005/006 `MapSr001Output`
path to generate 147 Final CP records and 1 Aggregate record.
The Python script creates a UUID EventId and UTC Timestamp, adds a header,
writes **149 JSONL records** to a unique file in the operating system's
temporary directory, and validates the artifact by reading it back.

The capture's positions and normals are converted back from O3DE Z-up
to the canonical coordinate system before serialization. The expected
normal is `(0,1,0)`. This matches the Unity BF-008 measurement semantics.

Expected key log values:

~~~text
SANSA_O3DE|BF-008.DISCOVERY|CPP_CAPTURE_AVAILABLE
SANSA_O3DE|BF-008.CPP_CP_COUNT|147
SANSA_O3DE|BF-008.CPP_CONTACT_COUNT|147
SANSA_O3DE|BF-008.CPP_SUPPORT_COUNT|14
SANSA_O3DE|BF-008.CPP_MEASUREMENTS_RESULT|PASS
SANSA_O3DE|BF-008.PYTHON_BRIDGE_RESULT|PASS
SANSA_O3DE|BF-008.VCF-001.RESULT|PASS
SANSA_O3DE|BF-008.VCF-002.RESULT|PASS
SANSA_O3DE|BF-008.VCF-003.RESULT|PASS
SANSA_O3DE|BF-008.VCF-004.RESULT|PASS
SANSA_O3DE|BF-008.VCF-005.RESULT|PASS
SANSA_O3DE|BF-008.VCF-006.RESULT|PASS
SANSA_O3DE|BF-008.VCF-007.RESULT|PASS
SANSA_O3DE|BF-008.TEST_ID|O3DE-SR-001-C0-G0
SANSA_O3DE|BF-008.RECORD_COUNT|149
SANSA_O3DE|BF-008.CP_COUNT|147
SANSA_O3DE|BF-008.CONTACT_COUNT|147
SANSA_O3DE|BF-008.SUPPORT_COUNT|14
SANSA_O3DE|BF-008.RESULT|PASS
SANSA_O3DE|BF-008.PYTHON_PROBE_RESULT|PASS
~~~

The log also prints `EVENT_ID`, `TIMESTAMP_UTC`, `BACKEND`,
`CAPTURE_PATH` and the aggregate metrics. Confirm the file exists and
contains 149 JSONL lines. If possible, attach the JSONL artifact as
evidence together with the Editor log.

BF-008 was confirmed **RUNTIME PASS** in O3DE Editor on 2026-10-09 JST
(2026-10-08 15:00:21 UTC). C++ measurements and Python bridge passed;
VCF-001 through VCF-007 all passed after JSONL file read-back.
149 records, 147 CP, 147 contacts, 14 supports, all separation and
deviation aggregates zero. The generated file itself was not attached
for independent review; the read-back result is established by the log.

O3DE PC BF-001 through BF-008 are now PASS at the validation-only
Runtime Backend Feasibility Checkpoint. This does not validate a
production solver, actual O3DE Mesh API or production serialization.

If the Gem or capture method is not exposed in a subsequent run,
the script reports OPEN; any capture or read-back error reports FAIL.

## Run OXF-001..010 Reference Fixture Exchange import

This stage uses the **committed Rust Reference resolved JSON**, not an
O3DE-generated analytic shape. It is a validation-only Python importer,
not an O3DE Mesh API or C++ solver integration.

After closing Editor, deploy with `BuildProbeAssets.ps1`. The deploy
script copies the baseline from
`reference/validation/fixture-exchange/basic-v1/SR-001-C0-G0.json`
to `<O3DE project>/Editor/Scripts/SansaCloth/Fixtures/SR-001-C0-G0.json`.
It compares the source and deployed SHA-256 and reports
`SCRIPT_COUNT|7`, `FIXTURE_SHA256`, `RESULT|PASS`.

Run in O3DE Editor > Tools > Other > Python Scripts:

~~~text
SansaCloth/sansacloth_fixture_exchange_import_probe.py
~~~

The script imports the JSON, strictly validates the format, IDs,
SurfaceReferences, topology, Anchor/Contact semantics and case inputs,
then maps canonical (x,y,z) to O3DE (x,z,y) and reverses triangle winding.
All 240 mapped triangles must be nondegenerate and outward-facing.
It also runs 14 negative importer tests.

Expected summary:

~~~text
SANSA_O3DE|OXF-001.RESULT|PASS
SANSA_O3DE|OXF-002.RESULT|PASS
SANSA_O3DE|OXF-003.RESULT|PASS
SANSA_O3DE|OXF-004.RESULT|PASS
SANSA_O3DE|OXF-005.RESULT|PASS
SANSA_O3DE|OXF-006.RESULT|PASS
SANSA_O3DE|OXF-007.RESULT|PASS
SANSA_O3DE|OXF-008.REJECTED_CASE_COUNT|14
SANSA_O3DE|OXF-008.RESULT|PASS
SANSA_O3DE|OXF-009.RESULT|PASS
SANSA_O3DE|OXF-010.RESULT|PASS
SANSA_O3DE|OXF.BODY_VERTEX_COUNT|147
SANSA_O3DE|OXF.BODY_TRIANGLE_COUNT|240
SANSA_O3DE|OXF.CP_COUNT|147
SANSA_O3DE|OXF.ANCHOR_COUNT|14
SANSA_O3DE|OXF.CONTACT_INPUT_COUNT|147
SANSA_O3DE|OXF.CONTACT_ONLY_UNSUPPORTED_COUNT|133
SANSA_O3DE|OXF.CASE_ID|SR-001-C0-G0
SANSA_O3DE|OXF.RESULT|PASS
SANSA_O3DE|OXF.PYTHON_PROBE_RESULT|PASS
~~~

The importer also reports the deployed source path and SHA-256.
A missing baseline reports OPEN; malformed or inconsistent data reports
FAIL. OXF-001..010 remain OPEN until real Editor runtime logs are
reviewed. Subsequent gates will validate the 30-case matrix and C++
backend integration separately.
