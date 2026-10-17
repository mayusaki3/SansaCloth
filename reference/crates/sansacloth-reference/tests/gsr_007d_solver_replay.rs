//! Replays SR-004-C1-G0 through the actual Rust solver using fixture-exchange inputs.
//! The committed snapshot is a legacy comparison baseline, not the solver's oracle.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    AnalyticFixtureSurfaceQuery, GravitySupportLayout, ReferenceSolverSettings,
    ReferenceSurfaceSolver, ReferenceSurfaceSolverInput, SurfaceQuery,
};
use serde_json::Value;

fn vector(value: &Value) -> DVec3 {
    let a = value.as_array().expect("3-component vector");
    assert_eq!(a.len(), 3);
    DVec3::new(a[0].as_f64().unwrap(), a[1].as_f64().unwrap(), a[2].as_f64().unwrap())
}

#[test]
fn gsr_007d_replay_fixture_through_current_rust_solver() {
    let fixture: Value = serde_json::from_str(include_str!(
        "../../../validation/fixture-exchange/basic-v1/SR-004-C1-G0.json"
    )).unwrap();
    let legacy: Value = serde_json::from_str(include_str!(
        "../../../validation/surface-response-result/basic-v1/SR-004-C1-G0.json"
    )).unwrap();
    assert_eq!(fixture["case_id"], legacy["case_id"]);
    let input_points = fixture["cloth"]["control_points"].as_array().unwrap();
    let saved_points = legacy["control_points"].as_array().unwrap();
    assert_eq!(input_points.len(), 147);
    assert_eq!(saved_points.len(), input_points.len());

    let query = AnalyticFixtureSurfaceQuery { fixture: AnalyticFixture::concave_shallow() };
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: legacy["profile"]["quasi_static_gravity_scale"].as_f64().unwrap(),
        conformity_reach_m: legacy["profile"]["conformity_reach_m"].as_f64().unwrap(),
        collision_tolerance_m: fixture["inputs"]["collision_tolerance_m"].as_f64().unwrap(),
    };
    let gravity = vector(&fixture["inputs"]["world_gravity_m_per_s2"]);
    let conformity = fixture["inputs"]["conformity"].as_f64().unwrap();
    let characteristic_length_m = legacy["profile"]["characteristic_length_m"].as_f64().unwrap();
    assert_eq!(legacy["profile"]["support_layout"].as_str(), Some("BothEdges"));
    let mut mismatches = [0usize; 9];
    let mut max_errors = [0.0f64; 9];
    let fields = [
        "bridge_position_m", "gravity_position_m", "conformity_position_m",
        "collision_position_m", "final_position_m", "surface_position_m",
        "surface_normal", "separation_m", "contact",
    ];
    for row in 0..7 {
        let start = row * 21;
        let input = &input_points[start..start + 21];
        let debug = ReferenceSurfaceSolver::solve_with_debug(
            &ReferenceSurfaceSolverInput {
                positions_m: input.iter().map(|p| vector(&p["position_m"])).collect(),
                surface_references: input.iter().map(|p| {
                    let sr = &p["surface_reference"];
                    SurfaceReference::new(
                        sr["domain_id"].as_u64().unwrap() as u32,
                        sr["u"].as_f64().unwrap(),
                        sr["v"].as_f64().unwrap(),
                    ).unwrap()
                }).collect(),
                is_anchor: input.iter().map(|p| p["anchor"].as_bool().unwrap()).collect(),
                is_contact: input.iter().map(|p| p["contact"].as_bool().unwrap()).collect(),
                gravity_m_per_s2: gravity,
                characteristic_length_m,
                support_layout: GravitySupportLayout::BothEdges,
                conformity,
                supported_strip: vec![true; 21],
            },
            &query,
            settings,
        );
        for col in 0..21 {
            let index = start + col;
            let saved = &saved_points[index];
            assert_eq!(saved["stable_id"].as_u64(), Some(index as u64));
            let final_result = &debug.final_result;
            let positions = [
                debug.bridge_positions_m[col],
                debug.gravity_positions_m[col],
                debug.conformity_positions_m[col],
                debug.collision_positions_m[col],
                final_result.positions_m[col],
                final_result.surface_positions_m[col],
                final_result.surface_normals[col],
            ];
            for (field_index, computed) in positions.into_iter().enumerate() {
                let error = computed.distance(vector(&saved[fields[field_index]]));
                if error > 1.0e-6 { mismatches[field_index] += 1; }
                max_errors[field_index] = max_errors[field_index].max(error);
            }
            let separation_error =
                (final_result.separation_m[col] - saved["separation_m"].as_f64().unwrap()).abs();
            if separation_error > 1.0e-6 { mismatches[7] += 1; }
            max_errors[7] = max_errors[7].max(separation_error);
            let computed_contact = final_result.separation_m[col] <= settings.collision_tolerance_m;
            if computed_contact != saved["contact"].as_bool().unwrap() {
                mismatches[8] += 1;
                max_errors[8] = 1.0;
            }
            let reference = final_result.surface_references[col];
            let recomputed = query.query(final_result.positions_m[col], reference);
            assert!(final_result.surface_normals[col].distance(recomputed.surface_normal) < 1.0e-12);
            assert!((final_result.separation_m[col] - recomputed.separation_m).abs() < 1.0e-12);
        }
    }
    for (i, field) in fields.iter().enumerate() {
        println!("GSR-007D solver replay | {field} | mismatches={}/147 | max_error={:.12}", mismatches[i], max_errors[i]);
    }
    // A baseline difference is expected while the old snapshot uses chord normals.
    assert!(mismatches[6] > 0, "legacy normal mismatch unexpectedly disappeared");
}
