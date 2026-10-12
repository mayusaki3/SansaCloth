# OPM-001 Mesh Discovery Fixture (O3DE 26.05)

## Purpose

A fixed, read-only rigid mesh for validating actual O3DE Atom Mesh discovery.
This fixture is **not** a skinned mesh, production SDF, or solver validation.
The OBJ is source geometry, not a prebuilt Atom ModelAsset.

## Source geometry

- Object: `SansaCloth_OPM001_Plane`
- O3DE local coordinates: +X right, +Y forward, +Z up (meters)
- Four authored positions: (-0.5,-0.5,0), (0.5,-0.5,0), (0.5,0.5,0), (-0.5,0.5,0)
- Two authored triangles: (1,2,3), (1,3,4), counterclockwise viewed from +Z
- Four UVs and one +Z normal
- Surface is 1 m × 1 m; origin at center.

**Important:** Imported Atom vertex/index counts are *not* assumed to equal OBJ source counts. Asset processing may change topology, vertex duplication, or LODs. Record actual counts from the probe.

## Editor setup (manual)

1. Pull `develop` and copy `opm001_plane.obj` into the **O3DE project's** `Assets/SansaCloth/OPM001/` directory (not the engine directory).
2. Wait for Asset Processor to import it. Check Asset Browser for the resulting model asset; inspect any import errors before proceeding.
3. In a validation level, create Entity `SansaCloth_OPM001_Plane` at translation (0,0,0), rotation (0,0,0), scale (1,1,1).
4. Add an Atom **Mesh** component and select the imported model asset. Confirm it is visible.
5. Save the level and select this Entity in Entity Outliner.
6. In Editor Python Console, run:

```python
import azlmbr.bus
import azlmbr.editor as editor
import sansacloth_probe

selected = editor.ToolsApplicationRequestBus(
    azlmbr.bus.Broadcast, "GetSelectedEntities"
)
print("OPM001|SELECTED_ENTITIES|", selected)
# ProbeMeshDiscovery currently expects a decimal EntityId string.
# If Python prints an EntityId wrapper instead of decimal digits, capture
# the output and stop: do not guess or strip digits from its representation.
```

7. Once a verified decimal EntityId is available, run `sansacloth_probe.ProbeMeshDiscovery(id_text)` and capture full output.

## OPM-001 test cases (defined before further production work)

| Case | Input | Expected |
| --- | --- | --- |
| OPM-001-TC-01 | OBJ import and Mesh Entity | Model asset ready; no Asset Processor error |
| OPM-001-TC-02 | Selected Mesh Entity | At least one LOD and mesh; positive vertex/index counts; POSITION view YES |
| OPM-001-TC-03 | Empty, nondecimal or zero ID | `INVALID_ENTITY_ID` (no crash) |
| OPM-001-TC-04 | Valid ID without ready ModelAsset | `MODEL_NOT_READY` (no crash) |
| OPM-001-TC-05 | Imported POSITION/index buffer access | **Not yet implemented**; verify descriptor, format, size and CPU data before interpreting |
| OPM-001-TC-06 | Entity reload / asset lifetime | **Not yet implemented**; verify model invalidation and readiness changes |

## Gate

OPM-001 stays **OPEN** until the actual O3DE 26.05 Editor build and
runtime logs demonstrate mesh discovery and safe CPU buffer access.
A successful Gem compile or OBJ import alone is insufficient.
