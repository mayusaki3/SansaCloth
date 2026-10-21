# GSR-007L — Curved integrated Reference Solver parity

Status: Rust snapshot generator PASS (2026-10-21); Godot parity implementation/execution pending.

## Purpose
Compare Rust ReferenceSurfaceSolver::solve_with_debug against a Godot implementation that independently computes Support → Bridge → Gravity → Surface Query → Conformity → Surface Query → Collision → Surface Query. Never use Rust snapshot intermediate outputs as computation inputs on the Godot side.

## Confirmed Rust v1 semantics
- Support: only explicit is_anchor yields Anchor; contact alone does not anchor.
- Bridge: both endpoints Anchor -> interpolate interior positions by index/(N-1); otherwise preserve initial positions.
- Gravity: zero vector => no movement; nonzero vector supplies direction only, not physical acceleration integration. For each unsupported point, displacement = normalized_gravity * characteristic_length_m * quasi_static_gravity_scale * weight; weight = 4t(1-t) for BothEdges or t for OneEdge; anchors remain unchanged.
- Surface Query after gravity: analytic fixture position and outward normal at SurfaceReference; separation = dot(current - surface, normal).
- Conformity: no movement if conformity=0, reach=0, Anchor, unsupported_strip, separation<0 or separation>=reach. Otherwise interpolate current position toward surface by effective=conformity*clamp(1-separation/reach,0,1).
- Surface Query after conformity; Collision pushes outward by (collision_tolerance-separation)*normal when separation < collision_tolerance. Query again for final result.
- Contact is derived from final separation and collision tolerance using is_surface_contact and CONTACT_EPSILON_M; it is not a support/Anchor condition.

## Test matrix
- Fixtures: Convex (+0.03 m), Concave Shallow (-0.02 m), Concave Deep (-0.05 m).
- One ordered 9-point strip per case: u=[0,0.20,0.25,0.375,0.5,0.625,0.75,0.80,1.0], v=0.5.
- Support layouts: BothEdges (anchors at 0 and 8), OneEdge (anchor only at 0).
- Gravity: zero; (0,-9.80665,0).
- Conformity: 0.0; 0.5; 1.0.
- Total 3*2*2*3 = 36 cases, 324 points.
- Settings: quasi_static_gravity_scale=0.1, characteristic_length_m=0.10, conformity_reach_m=0.02, collision_tolerance_m=0.005.
- Initial cloth points: analytic surface position plus outward normal * 0.015 m, with midpoint u=0.5 moved inward by 0.025 m to exercise penetration and bridge replacement.
- All supported_strip=true for the baseline. Follow-on negative coverage should include false and contact-only flags.

## Outputs / acceptance
Rust generator serializes case parameters and initial positions as inputs plus stage outputs: support, bridge, gravity, conformity, collision, final position, final surface position, final normal, final separation, contact. Godot independently recomputes each stage from parameters and initial positions, comparing all outputs. Verify expected case IDs and unique stable IDs; require exactly 36 cases/324 points, zero mismatches, exit code 0. Vector and signed separation tolerance 1e-6 m; support and contact exact. Report maximum per-stage position errors and separation/normal errors.

## Scope limitation
This is analytic fixture reference parity, not production Godot solver or mesh closest-point query parity. Nonzero Gravity and Conformity must cause measurable movement in at least one case each; tests must assert this to avoid vacuous PASS.

## Execution record
2026-10-21 Windows Rust: `cargo test -p sansacloth-reference --test gsr_007l_integrated_snapshot -- --ignored --nocapture` PASS; 36 cases, 324 points, `GRAVITY_MOVED=true`, `CONFORMITY_MOVED=true`. Generated `reference/validation/surface-response-result/basic-v1/GSR-007L-integrated.json` locally, not yet committed. Godot independent parity has not been run; do not claim full GSR-007L PASS.
