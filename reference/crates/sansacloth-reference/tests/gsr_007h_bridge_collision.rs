//! GSR-007H: Bridge input propagation, penetration correction, and contact boundary.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    is_surface_contact, AnalyticFixtureSurfaceQuery, GravitySupportLayout,
    ReferenceSolverSettings, ReferenceSurfaceSolver, ReferenceSurfaceSolverInput,
    SurfaceQuery, SupportKind, CONTACT_EPSILON_M,
};

const EPS: f64 = 1.0e-12;
const TOLERANCE: f64 = 0.005;
const INPUT_OFFSET: f64 = 0.030;

fn close(actual: f64, expected: f64) {
    assert!(
        (actual - expected).abs() <= EPS,
        "expected {expected:.12}, got {actual:.12}"
    );
}

fn close_vec(actual: DVec3, expected: DVec3) {
    assert!(
        actual.distance(expected) <= EPS,
        "expected {expected:?}, got {actual:?}"
    );
}

fn setup(two_edges: bool) -> (
    ReferenceSurfaceSolverInput,
    AnalyticFixtureSurfaceQuery,
    ReferenceSolverSettings,
) {
    let query = AnalyticFixtureSurfaceQuery {
        fixture: AnalyticFixture::flat(),
    };
    let references = [0.0, 0.5, 1.0]
        .map(|u| SurfaceReference::new(1, u, 0.5).unwrap());
    let baseline = references.map(|reference| {
        let surface = query.query(DVec3::ZERO, reference).surface_position_m;
        surface + DVec3::Y * 0.020
    });
    let mut positions = baseline;
    positions[1].y -= INPUT_OFFSET;
    let input = ReferenceSurfaceSolverInput {
        positions_m: positions.to_vec(),
        surface_references: references.to_vec(),
        is_anchor: vec![true, false, two_edges],
        is_contact: vec![false; 3],
        gravity_m_per_s2: DVec3::ZERO,
        characteristic_length_m: 0.1,
        support_layout: if two_edges {
            GravitySupportLayout::BothEdges
        } else {
            GravitySupportLayout::OneEdge
        },
        conformity: 0.0,
        supported_strip: vec![true; 3],
    };
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.1,
        conformity_reach_m: 0.02,
        collision_tolerance_m: TOLERANCE,
    };
    (input, query, settings)
}

#[test]
fn gsr_007h_c1_one_edge_preserves_displaced_input() {
    let (input, query, settings) = setup(false);
    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
    assert_eq!(
        debug.support,
        vec![
            SupportKind::Anchor,
            SupportKind::Unsupported,
            SupportKind::Unsupported,
        ]
    );
    close_vec(debug.bridge_positions_m[1], input.positions_m[1]);
    close_vec(debug.gravity_positions_m[1], input.positions_m[1]);
    close_vec(debug.conformity_positions_m[1], input.positions_m[1]);
    assert!(debug.bridge_positions_m.iter().all(|p| p.is_finite()));
}

#[test]
fn gsr_007h_c2_two_edges_replace_displaced_input() {
    let (input, query, settings) = setup(true);
    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
    assert_eq!(debug.support[2], SupportKind::Anchor);
    let expected = input.positions_m[0].lerp(input.positions_m[2], 0.5);
    close_vec(debug.bridge_positions_m[1], expected);
    close(debug.bridge_positions_m[1].y - input.positions_m[1].y, INPUT_OFFSET);
    close_vec(debug.bridge_positions_m[0], input.positions_m[0]);
    close_vec(debug.bridge_positions_m[2], input.positions_m[2]);
    close_vec(debug.gravity_positions_m[1], expected);
    close_vec(debug.conformity_positions_m[1], expected);
}

#[test]
fn gsr_007h_c3_penetration_survives_until_collision() {
    let (input, query, settings) = setup(false);
    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
    let reference = input.surface_references[1];
    let before = debug.conformity_positions_m[1];
    let pre_query = query.query(before, reference);
    assert!(pre_query.separation_m < 0.0, "interior must penetrate");
    close(pre_query.separation_m, -0.010);
    let after = debug.collision_positions_m[1];
    let expected_correction = settings.collision_tolerance_m - pre_query.separation_m;
    close_vec(after - before, pre_query.surface_normal * expected_correction);
    close(expected_correction, 0.015);
    close(debug.final_result.separation_m[1], TOLERANCE);
    close_vec(debug.final_result.positions_m[1], after);
    assert!(
        (debug.collision_positions_m[1] - debug.conformity_positions_m[1]).length() > EPS
    );
}

#[test]
fn gsr_007h_c4_contact_boundary_and_collision_threshold_are_distinct() {
    let t = TOLERANCE;
    let e = CONTACT_EPSILON_M;
    for (separation, expected) in [
        (t - 0.0001, true),
        (t, true),
        (t + e * 0.5, true),
        (t + e, true),
        (t + e * 2.0, false),
    ] {
        assert_eq!(
            is_surface_contact(separation, t),
            expected,
            "separation={separation}"
        );
    }

    // Contact can be true without collision correction: epsilon does not enlarge
    // the collision tolerance.
    let (mut input, query, settings) = setup(false);
    let middle_surface = query
        .query(DVec3::ZERO, input.surface_references[1])
        .surface_position_m;
    input.positions_m[1] = middle_surface + DVec3::Y * (t + e * 0.5);
    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
    close_vec(debug.conformity_positions_m[1], debug.collision_positions_m[1]);
    assert!(is_surface_contact(debug.final_result.separation_m[1], t));
    assert!(debug.final_result.separation_m[1] > t);
}
