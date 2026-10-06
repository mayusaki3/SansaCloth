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
SANSA_O3DE_BUILD|RESULT|PASS
~~~

The deploy replaces only:

~~~text
<O3DE project>\Editor\Scripts\SansaCloth
~~~

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
