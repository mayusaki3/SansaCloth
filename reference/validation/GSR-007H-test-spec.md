# GSR-007H — Bridge input propagation and collision validation

Status: Test specification / implementation pending
Target: SansaCloth Rust Reference v1 and Godot validation
Predecessor: GSR-007G-C1 (147 points, Rust–Godot parity PASS)
Units: metres; X right, Y up, Z forward.

## Purpose

Verify whether a deliberate cloth input displacement survives the Bridge stage and reaches Collision. GSR-007G applies -0.030 m to interior inputs but its two supported endpoints cause Bridge to replace interior positions, so GSR-007G alone does not demonstrate input-displacement-driven collision.

## Observed Reference v1 contracts

1. `resolve_support` determines support from anchor/contact flags.
2. `resolve_bridge` replaces interior positions with linear endpoint interpolation only when both endpoints have `SupportKind != Unsupported`. With fewer than two supported endpoints, it preserves all input positions.
3. Gravity moves only unsupported points. `OneEdge` weights are `t`; `BothEdges` weights are `4*t*(1-t)`.
4. Conformity operates on unsupported, eligible points with nonnegative separation and within reach. It does not itself correct penetration.
5. Collision corrects points with `separation_m < collision_tolerance_m` along normalized outward surface normal.
6. Contact classification for GSR-007G parity uses `separation_m <= collision_tolerance_m + CONTACT_EPSILON_M`, with `CONTACT_EPSILON_M = 1e-6 m`. This does not change the collision correction threshold.

## Common setup

Use deterministic, explicitly identified strip points, with stable IDs and surface references. Prefer a flat horizontal analytic surface for exact stage assertions, plus a separate concave-surface parity variant if needed. Use at least three strip points (left endpoint, displaced interior, right endpoint). For isolated Bridge and Collision assertions, disable Gravity (zero vector) and Conformity (zero scalar). Set collision tolerance to 0.005 m. Choose a nonzero interior input displacement of -0.030 m and initial separation that makes the resulting point penetrate the body surface. Compare all stage snapshots and final signed separation.

For end-to-end cross-engine cases, Rust generates a committed result snapshot; Godot independently computes results from a shared fixture rather than consuming Rust intermediate positions as inputs.

## Test cases

### GSR-007H-C1 — One-edge input preservation

- Left endpoint: anchor; right endpoint: unsupported and not contact; interior: unsupported and not contact.
- Input: interior position displaced by -0.030 m in Y relative to a declared baseline.
- Gravity: zero; Conformity: zero; support layout: OneEdge.
- Assert: interior Bridge position equals displaced input position, within numerical tolerance; input displacement is not erased.
- Assert: the right endpoint is not implicitly treated as supported.
- Assert: intermediate stage values are finite.

### GSR-007H-C2 — Two-edge input replacement

- Both endpoints: anchor; interior: unsupported.
- Same baseline geometry and -0.030 m interior displacement as C1.
- Gravity: zero; Conformity: zero; support layout: BothEdges.
- Assert: interior Bridge position equals linear interpolation of the endpoint positions.
- Assert: interior Bridge position differs from the displaced input by the expected offset, with explicit nonzero difference.
- Assert: endpoint positions are unchanged by Bridge.

### GSR-007H-C3 — Penetration survives to Collision

- Same support setup as C1.
- Place the displaced interior input strictly inside the body surface (signed separation < 0).
- Gravity: zero; Conformity: zero in the isolated variant.
- Assert: signed separation immediately before Collision remains negative.
- Assert: the exact displaced interior stable ID is corrected, not merely that some point was corrected.
- Assert: correction direction follows the outward normal and magnitude is `collision_tolerance_m - pre_collision_separation_m`.
- Assert: final signed separation is at the collision tolerance within numerical precision.
- Extended variant: enable nonzero Gravity and Conformity and assert measured stage-by-stage propagation without assuming that these stages preserve the input displacement.

### GSR-007H-C4 — Contact boundary and independent parity

- Use explicit signed separations on both sides of `tolerance` and `tolerance + CONTACT_EPSILON_M`.
- Assert Rust and Godot agree on Contact boolean for each case; no ambiguous mismatch is ignored.
- Assert Collision correction uses exact tolerance, not Contact epsilon.
- Assert equality and just-outside-boundary cases using representable test values separated sufficiently to avoid f32 rounding ambiguity, or compute boundary values in the engine's native precision and document them.
- Require no Contact mismatches.

## Cross-engine acceptance criteria

- Stable IDs and case IDs match exactly; no duplicate or missing IDs.
- Compare Bridge, Gravity, Conformity, Collision and Final Position, final separation and Contact.
- Position/separation parity: maximum absolute error <= 1e-6 m, while exact Rust stage invariants should use a tighter tolerance (e.g. 1e-12 m for f64 analytic flat-surface checks).
- Collision-corrected flags and Contact booleans match exactly; no boundary mismatch exception.
- C1: displaced input preserved by Bridge.
- C2: displaced input overwritten by Bridge interpolation.
- C3: intended penetrating stable ID has negative pre-collision separation and a nonzero outward correction.
- C4: boundary classification agrees and correction threshold remains exact.
- Test process returns nonzero on any failed invariant; Rust tests and Godot headless validation must both pass.

## Implementation sequence

1. Add Rust tests that directly assert stage invariants; keep them independent of snapshot regeneration.
2. Add a deterministic GSR-007H fixture and an explicitly invoked Rust snapshot generator.
3. Add a separate Godot headless parity test that independently evaluates fixture inputs.
4. Verify Rust tests, regenerate and review snapshot, execute Godot test, then commit generated artifacts.
5. Record actual counts, maximum errors and exit code. Do not mark GSR-007H PASS until executed.

## Scope limits

- Do not modify GSR-007G snapshots or its existing parity test.
- Do not remove unrelated untracked diagnostic outputs.
- This test establishes Reference v1 behavior; it does not claim dynamic cloth simulation or physical time integration.
- HLDocS v0.6.1 detailed schema/document-type conformity requires checking the canonical HLDocS templates separately before labeling this document formally compliant.
