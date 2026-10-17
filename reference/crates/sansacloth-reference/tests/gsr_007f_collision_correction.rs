//! GSR-007F: deterministic collision correction cases with explicit expected positions.
//! Anchor status is intentionally not a collision-stage input in Reference v1.
use glam::DVec3;
use sansacloth_reference::{
    apply_collision_response, CollisionResponseInput, ReferenceSolverSettings,
};

#[test]
fn gsr_007f_collision_correction_matrix() {
    // Case IDs are shared with the Godot GSR-007F probe.
    let cases: [(&str, DVec3, DVec3, f64, f64, DVec3, bool); 7] = [
        ("penetration_zero", DVec3::new(0.0, -0.01, 0.0), DVec3::Y, -0.01, 0.0, DVec3::ZERO, true),
        ("penetration_tolerance", DVec3::new(0.0, -0.01, 0.0), DVec3::Y, -0.01, 0.005, DVec3::new(0.0, 0.005, 0.0), true),
        ("outside_below_tolerance", DVec3::new(0.0, 0.002, 0.0), DVec3::Y, 0.002, 0.005, DVec3::new(0.0, 0.005, 0.0), true),
        ("at_tolerance", DVec3::new(0.0, 0.005, 0.0), DVec3::Y, 0.005, 0.005, DVec3::new(0.0, 0.005, 0.0), false),
        ("outside_tolerance", DVec3::new(0.0, 0.01, 0.0), DVec3::Y, 0.01, 0.005, DVec3::new(0.0, 0.01, 0.0), false),
        ("anchor_penetration", DVec3::new(0.0, -0.004, 0.0), DVec3::Y, -0.004, 0.0, DVec3::ZERO, true),
        ("diagonal_unnormalized", DVec3::new(-0.01, 0.0, 0.0), DVec3::new(2.0, 0.0, 0.0), -0.01, 0.005, DVec3::new(0.005, 0.0, 0.0), true),
    ];
    let mut corrected = 0;
    for (id, position, normal, separation, tolerance, expected, should_correct) in cases {
        let output = apply_collision_response(
            &CollisionResponseInput {
                positions_m: vec![position],
                separation_m: vec![separation],
                surface_normals: vec![normal],
            },
            ReferenceSolverSettings {
                quasi_static_gravity_scale: 0.1,
                conformity_reach_m: 0.02,
                collision_tolerance_m: tolerance,
            },
        )[0];
        assert!(output.distance(expected) <= 1.0e-12, "{id}: actual={output:?}, expected={expected:?}");
        let did_correct = output.distance(position) > 1.0e-12;
        assert_eq!(did_correct, should_correct, "{id}: correction state");
        corrected += usize::from(did_correct);
        println!("GSR-007F | {id} | corrected={did_correct} | error={:.12}", output.distance(expected));
    }
    assert_eq!(corrected, 5);
    println!("GSR-007F | CHECKED=7 | CORRECTED={corrected} | PASS");
}
