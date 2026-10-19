# GSR-007I — Analytic Surface Query independent Rust–Godot parity

Status: Implementation added; execution pending
Predecessor: GSR-007H isolated flat-fixture stage parity PASS
Units: metres; +X right, +Y up, +Z forward

## Purpose

Verify that Godot independently evaluates the analytic body fixture position, outward normal and signed separation from fixture kind, (u,v), and a current cloth position. The Godot test MUST NOT read Rust surface positions or normals as calculation inputs.

## Reference formulas

Fixture width = 0.20 m; depth = 0.10 m; feature width = 0.10 m.
x=(u-0.5)*0.20; z=(v-0.5)*0.10.
For |x| > 0.05, height and derivative are zero.
Within the feature: raised cosine h(x)=amplitude*0.5*(1+cos(2*pi*x/0.10)).
Derivative h'(x)=-(amplitude*pi/0.10)*sin(2*pi*x/0.10).
Flat: y=0, derivative=0.
Convex: y=h(x), derivative=h'(x), amplitude=+0.03.
Concave Shallow: y=-h(x), derivative=-h'(x), amplitude=+0.02.
Concave Deep: y=-h(x), derivative=-h'(x), amplitude=+0.05.
Outward normal=normalize((-derivative,1,0)).
Separation=dot(current_position-surface_position, outward_normal).

## Cases and inputs

Use 4 fixture kinds × 7 u coordinates (0, 0.20, 0.25, 0.375, 0.50, 0.75, 1.0) × 2 v coordinates (0.25, 0.75) × 3 offsets (-0.01, 0, +0.02) = 168 probes. Current position is the analytic surface position plus the analytic outward normal times the declared offset. Stable probe IDs are sequential and deterministic.

Rust generates a JSON snapshot with only fixture kind, (u,v), current position, and expected query outputs. Godot independently computes the fixture and compares all three query outputs. Comparison tolerance: 1e-6 m for position and separation, 1e-6 for normal Euclidean distance. Any mismatch or malformed fixture fails the headless process. The test reports maximum observed errors.

## Acceptance

- Exactly 168 probes, stable IDs unique and sequential, all 4 fixture kinds present.
- Surface position, outward normal and signed separation match the Rust snapshot within tolerance.
- Godot exits 0 only when mismatch count is 0.
- Existing GSR-007G/H snapshots remain unchanged.
- This validates analytic fixtures, not a mesh-based closest-point query or production Godot solver.

## Execution record

Pending local Rust snapshot generation and Godot headless execution. Do not mark PASS until both have run.
