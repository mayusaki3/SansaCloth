# Godot Solver Reference Validation — test-first plan

Status: SPECIFIED / NOT RUN. Target: Godot 4.7.2, Compatibility renderer.

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

## Execution workflow (planned, not implemented)

1. Editor Tools menu: Setup Validation Scene / Run Solver Tests / Open Validation Scene.
2. CLI: Godot headless project invocation of a test runner that loads fixture JSON and writes a report.
3. Interactive scene remains usable after a test run.

## Open questions to resolve from source

- Exact Rust fixture JSON input/output schema and stable-ID semantics.
- Collision/penetration response equations and ordering.
- Godot numerical precision and acceptable comparison tolerance.
- Existing Unity probe fixture paths and test case coverage.

Do not infer answers to these from the O3DE report alone.
