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
SANSA_O3DE_BUILD|SCRIPT_COUNT|1
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
