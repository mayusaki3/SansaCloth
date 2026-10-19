//! Explicit GSR-007H snapshot generation. Not run during ordinary cargo test.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    is_surface_contact, AnalyticFixtureSurfaceQuery, GravitySupportLayout,
    ReferenceSolverSettings, ReferenceSurfaceSolver, ReferenceSurfaceSolverInput, SurfaceQuery,
    SupportKind,
};

fn vector(v: DVec3) -> serde_json::Value {
    serde_json::json!([v.x, v.y, v.z])
}

#[test]
#[ignore = "writes the GSR-007H reference snapshot; run explicitly"]
fn gsr_007h_generate_snapshot() {
    let query = AnalyticFixtureSurfaceQuery { fixture: AnalyticFixture::flat() };
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.1,
        conformity_reach_m: 0.02,
        collision_tolerance_m: 0.005,
    };
    let mut cases = Vec::new();
    for (id, two_edges, boundary) in [
        ("GSR-007H-C1", false, false),
        ("GSR-007H-C2", true, false),
        ("GSR-007H-C3", false, false),
        ("GSR-007H-C4", false, true),
    ] {
        let refs = [0.0, 0.5, 1.0]
            .map(|u| SurfaceReference::new(1, u, 0.5).unwrap());
        let surface: Vec<_> = refs.iter()
            .map(|&r| query.query(DVec3::ZERO, r))
            .collect();
        let mut positions: Vec<DVec3> = surface.iter()
            .map(|q| q.surface_position_m + q.surface_normal * 0.020)
            .collect();
        let middle_separation = if boundary { 0.0050005 } else { -0.010 };
        positions[1] = surface[1].surface_position_m
            + surface[1].surface_normal * middle_separation;
        let input = ReferenceSurfaceSolverInput {
            positions_m: positions.clone(),
            surface_references: refs.to_vec(),
            is_anchor: vec![true, false, two_edges],
            is_contact: vec![false; 3],
            gravity_m_per_s2: DVec3::ZERO,
            characteristic_length_m: 0.1,
            support_layout: if two_edges { GravitySupportLayout::BothEdges }
                else { GravitySupportLayout::OneEdge },
            conformity: 0.0,
            supported_strip: vec![true; 3],
        };
        let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
        let points: Vec<_> = (0..3).map(|i| serde_json::json!({
            "stable_id": i,
            "anchor": input.is_anchor[i],
            "initial_position_m": vector(input.positions_m[i]),
            "surface_position_m": vector(debug.final_result.surface_positions_m[i]),
            "surface_normal": vector(debug.final_result.surface_normals[i]),
            "bridge_position_m": vector(debug.bridge_positions_m[i]),
            "gravity_position_m": vector(debug.gravity_positions_m[i]),
            "conformity_position_m": vector(debug.conformity_positions_m[i]),
            "collision_position_m": vector(debug.collision_positions_m[i]),
            "final_position_m": vector(debug.final_result.positions_m[i]),
            "separation_m": debug.final_result.separation_m[i],
            "contact": is_surface_contact(debug.final_result.separation_m[i], settings.collision_tolerance_m),
            "support": match debug.final_result.support[i] {
                SupportKind::Anchor => "Anchor",
                SupportKind::Unsupported => "Unsupported",
            }
        })).collect();
        cases.push(serde_json::json!({"case_id": id, "points": points}));
    }
    let document = serde_json::json!({
        "schema": "GSR-007H-parity-v1",
        "collision_tolerance_m": settings.collision_tolerance_m,
        "contact_epsilon_m": sansacloth_reference::CONTACT_EPSILON_M,
        "cases": cases
    });
    let relative = "../../validation/surface-response-result/basic-v1/GSR-007H-parity.json";
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(relative);
    let mut output = serde_json::to_string_pretty(&document).unwrap();
    output.push('\n');
    std::fs::write(&path, output).expect("write GSR-007H parity snapshot");
    println!("GSR-007H | SNAPSHOT={} | CASES=4 | PASS", path.display());
}
