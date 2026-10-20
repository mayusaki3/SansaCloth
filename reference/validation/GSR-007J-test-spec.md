# GSR-007J — Integrated analytic Surface Query to Collision parity

Status: Flat-fixture integrated parity PASS (2026-10-20); extended fixtures pending
Predecessors: GSR-007H flat stage parity PASS; GSR-007I analytic Surface Query 168/168 PASS.

## Purpose

Replace the GSR-007H Godot parity test's use of Rust snapshot surface positions and normals with independent analytic Surface Query evaluation. Keep the GSR-007H committed snapshot immutable and compare the independently calculated stage outputs against it.

## Scope

- Four GSR-007H cases, three points per case; analytic Flat fixture.
- Reconstruct the surface reference from the ordered strip stable ID: u=0,0.5,1; v=0.5. This mapping is currently implicit in GSR-007H snapshot generation and must be checked against its Rust generator.
- Evaluate surface position and normal from fixture constants (width 0.20 m, depth 0.10 m, flat height 0) at each stage's current position.
- Recompute signed separation before Collision and after Collision from the newly evaluated Surface Query, not from snapshot surface values.
- Compare surface position, normal, Bridge, Gravity, Conformity, Collision, final position, final separation, support, and contact to Rust snapshot.
- Require stable IDs, unique case IDs, expected case IDs and count, 0 mismatches, exit code 0; log max position/normal/separation errors.
- All vector/separation comparisons <= 1e-6 m; contact/support exact.
- Godot computes only Reference v1 fixture-specific behavior, not a production runtime solver.

## Non-goals

Do not modify GSR-007G/H/I snapshots. Do not claim this flat-only integration demonstrates independent convex/concave Collision parity or nonzero Gravity/Conformity. Later cases should extend the integrated pipeline across those inputs.

## Execution record

Godot 4.7.2 headless: 4 cases, 12 points, 0 mismatches, exit code 0. Maximum position error: 0 m; maximum normal error: 0; maximum separation error: 0.00000000044703 m. This PASS applies only to the flat-fixture, zero-gravity, zero-conformity integrated parity. Convex/concave and nonzero-response integration remain pending.
