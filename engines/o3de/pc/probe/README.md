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
SANSA_O3DE_BUILD|SCRIPT_COUNT|3
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
BF-003 remains OPEN until actual runtime evidence is recorded.

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
error. BF-004 remains OPEN until real Editor runtime evidence is recorded.
This does not validate production Mesh API, deforming surfaces, non-uniform
scale, or a full cloth solver.
