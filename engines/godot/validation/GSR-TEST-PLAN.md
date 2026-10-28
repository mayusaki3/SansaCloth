# Godot Solver Reference Validation — test-first plan

Status: PARTIALLY VERIFIED (GSR-007K / GSR-007L numerical parity PASS). Target: Godot 4.7.2, Compatibility renderer.

## Boundary

Use the Rust reference fixture exchange as the authoritative input and expected output. Godot must compute its own results; copying expected output, replaying reference results, or merely animating placeholders is not a solver pass.

Canonical axes: X right, Y up, Z forward. Units: metres. Preserve stable IDs and input ordering.

## Existing evidence

An O3DE OSR-008 evidence report records 30 PASS comparisons for analytic fixture IDs SR-001 through SR-005, with variants C0/C05/C1 and G0/G1. Its reported position and normal tolerance is 1e-12, but that threshold is NOT automatically adopted for Godot. The report is not proof of Godot execution.

## Tests to write before solver implementation

| ID | Input | Assertion | Gate |
| --- | --- | --- | --- |
| GSR-001 | Godot project/editor plugin | Scene is created, saved and reopened without overwriting an existing scene | Manual editor |
| GSR-002 | Analytic flat fixture with one penetrated vertex | Godot-computed correction moves vertex outside target surface according to reference semantics | Automated |
| GSR-003 | Flat fixture with nonpenetrating vertex | No spurious correction | Automated |
| GSR-004 | Convex analytic fixture | Computed corrected position matches Rust reference within an established tolerance | Automated |
| GSR-005 | Concave analytic fixture | Computed corrected position matches Rust reference within an established tolerance | Automated |
| GSR-006 | Gravity and transform variants | Explicit world-space gravity, body-local mapping and output agree with reference | Automated |
| GSR-007 | Fixture exchange batch | Each fixture is executed by Godot, output compared by stable ID; all mismatches reported | Automated |
| GSR-008 | Interactive scene | Pause/step/reset; actual solver output drives visible mesh; results survive test run | Manual + automated |

## Acceptance rules

- GSR-002–007 must not report PASS until fixture format, reference semantics and tolerance have been checked against current Rust code.
- Record Godot version, renderer, input fixture SHA256, output SHA256, tolerance, per-case maximum error and PASS/FAIL/OPEN.
- Reject malformed fixtures and mismatched IDs; do not silently skip cases.
- No test may pass solely because the scene launched.
- Test first, then implement the smallest Godot solver adapter needed for the selected fixture.
- Initially target GSR-002 and GSR-003. Extend to curved surfaces and gravity only after these pass.
- Keep reference fixtures in the repository; generated output should be excluded from source control.

## Execution workflow (headless CLI verified; editor workflow unverified)

1. Editor Tools menu: Setup Validation Scene / Run Solver Tests / Open Validation Scene (UNVERIFIED).
2. CLI: Godot headless execution of GSR-007K / GSR-007L parity scripts (VERIFIED; console results, exit code 0).
3. Interactive scene remains usable after a test run (UNVERIFIED).

## Open questions to resolve from source

- Exact Rust fixture JSON input/output schema and stable-ID semantics.
- Collision/penetration response equations and ordering.
- Godot numerical precision and acceptable comparison tolerance.
- Existing Unity probe fixture paths and test case coverage.

Do not infer answers to these from the O3DE report alone.

## GSR-007L verification result

Date: 2026-10-24
Status: PASS

- Godot: 4.7.2 stable
- Execution: Windows headless CLI
- Reference: Rust independent solver
- Cases: 36
- Points: 324
- Contacts: 61
- Gravity movement: confirmed
- Conformity movement: confirmed
- Comparison tolerance: 1e-6
- Maximum position error: 7.50856e-9 m
- Maximum separation error: 3.18371e-9 m
- Mismatches: 0
- Exit code: 0

The Godot test independently evaluates support,
bridge, gravity, surface query, conformity,
collision and final surface query.

Reference intermediate positions are used only
for comparison, not as solver inputs.

Test:
addons/sansacloth_validation/tests/gsr_007l_integrated_parity_test.gd

Reference snapshot:
addons/sansacloth_validation/reference-results/GSR-007L-integrated.json

Scope: analytic fixture numerical parity only.
Editor integration, real mesh integration and
cross-platform behavior remain unverified.

## GSR-001 verification result

Date: 2026-10-28
Status: PASS

- Godot: 4.7.2 stable
- Platform: Windows 11
- Renderer: Compatibility / OpenGL 3.3
- EditorPlugin load: PASS
- Scene generation: PASS
- Scene persistence: PASS
- Required scene nodes: PASS
- Existing scene detection: PASS
- Non-overwrite verification: PASS
- Editor restart: PASS

Scene:
res://validation/sansacloth_validation.tscn

SHA256:
92C3F4BE2B34BE0C291BB920D7D4EB34AA5A93956B91D24026E23EE36C115A46

Evidence:
SANSA_GODOT|SETUP|PASS
SANSA_GODOT|SETUP|EXISTS

The scene SHA256 remained unchanged after
restarting the Godot Editor and executing
Setup Validation Scene again.

Scope: EditorPlugin scene generation, persistence
and non-overwrite behavior only.

Interactive solver operation, real mesh integration
and cross-platform behavior remain unverified.