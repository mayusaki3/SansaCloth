//! Replays SR-004-C1-G0 through the actual Rust solver using fixture-exchange inputs.
//! The committed snapshot is regenerated from the current solver and is a parity oracle.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    AnalyticFixtureSurfaceQuery, GravitySupportLayout, ReferenceSolverSettings,
    ReferenceSurfaceSolver, ReferenceSurfaceSolverInput,
};
use serde_json::Value;

fn vector(value: &Value) -> DVec3 {
    let a = value.as_array().expect("3-component vector");
    assert_eq!(a.len(), 3);
    DVec3::new(a[0].as_f64().unwrap(), a[1].as_f64().unwrap(), a[2].as_f64().unwrap())
}

#[test]
#[ignore = "explicitly writes GSR-007G snapshot; run with --ignored"]
fn gsr_007g_generate_collision_snapshot() {

    let fixture: Value = serde_json::from_str(include_str!(
        "../../../validation/fixture-exchange/basic-v1/SR-004-C1-G0.json"
    )).unwrap();
    let mut document: Value = serde_json::from_str(include_str!(
        "../../../validation/surface-response-result/basic-v1/SR-004-C1-G0.json"
    )).unwrap();
    let input_points = fixture["cloth"]["control_points"].as_array().unwrap();
    let saved_points = document["control_points"].as_array_mut().unwrap();
    assert_eq!(input_points.len(), 147);
    assert_eq!(saved_points.len(), 147);

    let query = AnalyticFixtureSurfaceQuery { fixture: AnalyticFixture::concave_shallow() };
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.1,
        conformity_reach_m: 0.02,
        collision_tolerance_m: 0.005,
    };
    let gravity = vector(&fixture["inputs"]["world_gravity_m_per_s2"]);
    let conformity = fixture["inputs"]["conformity"].as_f64().unwrap();
    let vec_json = |v: DVec3| serde_json::json!([v.x, v.y, v.z]);
    let mut corrected = 0usize;
    for row in 0..7 {
        let start = row * 21;
        let input = &input_points[start..start + 21];
        let debug = ReferenceSurfaceSolver::solve_with_debug(
            &ReferenceSurfaceSolverInput {
                positions_m: input.iter().map(|p| { let mut v = vector(&p["position_m"]); let col = p["stable_id"].as_u64().unwrap() % 21; if (5..=15).contains(&col) { v.y -= 0.03; } v }).collect(),
                surface_references: input.iter().map(|p| {
                    let sr = &p["surface_reference"];
                    SurfaceReference::new(
                        sr["domain_id"].as_u64().unwrap(),
                        sr["u"].as_f64().unwrap(),
                        sr["v"].as_f64().unwrap(),
                    ).unwrap()
                }).collect(),
                is_anchor: input.iter().map(|p| p["anchor"].as_bool().unwrap()).collect(),
                is_contact: input.iter().map(|p| p["contact"].as_bool().unwrap()).collect(),
                gravity_m_per_s2: gravity,
                characteristic_length_m: 0.1,
                support_layout: GravitySupportLayout::BothEdges,
                conformity,
                supported_strip: vec![true; 21],
            },
            &query,
            settings,
        );
        for col in 0..21 {
            let index = start + col;
            if debug.collision_positions_m[col].distance(debug.conformity_positions_m[col]) > 1.0e-9 { corrected += 1; }
            let p = &mut saved_points[index];
            assert_eq!(p["stable_id"].as_u64(), Some(index as u64));
            p["bridge_position_m"] = vec_json(debug.bridge_positions_m[col]);
            p["gravity_position_m"] = vec_json(debug.gravity_positions_m[col]);
            p["conformity_position_m"] = vec_json(debug.conformity_positions_m[col]);
            p["collision_position_m"] = vec_json(debug.collision_positions_m[col]);
            p["final_position_m"] = vec_json(debug.final_result.positions_m[col]);
            p["surface_position_m"] = vec_json(debug.final_result.surface_positions_m[col]);
            p["surface_normal"] = vec_json(debug.final_result.surface_normals[col]);
            p["separation_m"] = serde_json::json!(debug.final_result.separation_m[col]);
            p["contact"] = serde_json::json!(
                debug.final_result.separation_m[col] <= settings.collision_tolerance_m
            );
            p["support"] = serde_json::json!(match debug.final_result.support[col] {
                sansacloth_reference::SupportKind::Anchor => "Anchor",
                sansacloth_reference::SupportKind::Unsupported => "Unsupported",
            });
        }
    }
    assert!(corrected > 0, "GSR-007G must exercise actual collision corrections");
    document["profile"]["collision_tolerance_m"] = serde_json::json!(0.005);
    document["case_id"] = serde_json::json!("GSR-007G-C1");
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../validation/surface-response-result/basic-v1/GSR-007G-C1.json");
    let mut content = serde_json::to_string_pretty(&document).unwrap();
    content.push('\n');
    std::fs::write(&path, content).expect("write regenerated snapshot");
    println!("GSR-007G | CORRECTED={corrected} | SNAPSHOT={}", path.display());
}
