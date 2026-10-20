# GSR-007K — Curved analytic Surface Query → Collision parity

Status: Analytic curved Surface Query → Collision parity PASS (2026-10-20); full solver integration pending
Predecessors: GSR-007I independent Surface Query PASS; GSR-007J flat integrated parity PASS.

## Objective

Verify that Godot independently evaluates curved fixture surface positions, outward normals and signed separations, and uses those calculations in the Collision stage. Rust snapshot surface outputs are comparison-only and MUST NOT be used as Godot calculation inputs.

## Fixtures and cases

Convex (height +0.03 m), Concave Shallow (depth 0.02 m), Concave Deep (depth 0.05 m). Each fixture is sampled at u=[0, 0.20, 0.25, 0.375, 0.5, 0.625, 0.75, 0.80, 1.0], v=0.5. For each surface reference, three cloth offsets are tested: -0.01 m penetration, +0.005 m exactly at collision tolerance, and +0.02 m outside. Total 3 × 9 × 3 = 81 probes.

Fixture analytic geometry: x=(u-0.5)*0.20; z=(v-0.5)*0.10; y=A/2*(1+cos(2πx/0.10)) for |x|<=0.05, otherwise 0; A=+0.03/-0.02/-0.05. Outward normal normalize((-dy/dx,1,0)). Signed separation dot(current-surface,normal).

Rust evaluates each probe with AnalyticFixtureSurfaceQuery and apply_collision_response, then independently queries the corrected position. Godot uses the same fixture kind/u/v/current position, independently computes surface query, applies collision correction with tolerance 0.005 m, then independently queries the corrected position.

Compare before/after surface positions, normals, signed separations, corrected position, and contact classification. Tolerance: 1e-6 for vector distances and signed separations; contact exact. Require 81 sequential unique stable IDs, 3 fixture kinds, 0 mismatches, exit 0. Print maximum errors.

## Limitations

This is Surface Query → Collision integration, not a full Bridge/Gravity/Conformity solver test. It uses analytic surface references, not mesh nearest-point projection. Do not modify prior snapshots.

## Execution record

Rust snapshot generation: 81 probes, PASS. Godot 4.7.2 headless: 81 probes, 0 mismatches, exit code 0. Maximum position error: 0.00000000093132 m; maximum normal error: 0; maximum separation error: 0.00000000208616 m. PASS scope: analytic curved Surface Query → Collision, not Bridge/Gravity/Conformity or production mesh query. Snapshot files are pending commit from the local validation environment.
