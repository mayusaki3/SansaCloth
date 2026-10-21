//! Explicit GSR-007L full-stage curved solver snapshot generation.
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
#[ignore = "writes GSR-007L reference snapshot; run explicitly"]
fn generate_gsr_007l_snapshot() {
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.1,
        conformity_reach_m: 0.02,
        collision_tolerance_m: 0.005,
    };
    let fixtures = [
        ("convex", AnalyticFixture::convex()),
        ("concave_shallow", AnalyticFixture::concave_shallow()),
        ("concave_deep", AnalyticFixture::concave_deep()),
    ];
    let samples = [0.0, 0.20, 0.25, 0.375, 0.5, 0.625, 0.75, 0.80, 1.0];
    let mut cases = Vec::new();
    let mut gravity_moved = false;
    let mut conformity_moved = false;

    for (fixture_kind, fixture) in fixtures {
        let query = AnalyticFixtureSurfaceQuery { fixture };
        for both_edges in [true, false] {
            for gravity_enabled in [false, true] {
                for conformity in [0.0, 0.5, 1.0] {
                    let references: Vec<_> = samples.iter().map(|&u| {
                        SurfaceReference::new(1, u, 0.5).unwrap()
                    }).collect();
                    let mut initial: Vec<_> = references.iter().map(|&reference| {
                        let surface = query.query(DVec3::ZERO, reference);
                        surface.surface_position_m + surface.surface_normal * 0.015
                    }).collect();
                    initial[4] -= query.query(DVec3::ZERO, references[4]).surface_normal * 0.025;
                    let anchors: Vec<_> = (0..samples.len()).map(|i| {
                        i == 0 || (both_edges && i == samples.len() - 1)
                    }).collect();
                    let gravity = if gravity_enabled {
                        DVec3::new(0.0, -9.80665, 0.0)
                    } else {
                        DVec3::ZERO
                    };
                    let input = ReferenceSurfaceSolverInput {
                        positions_m: initial,
                        surface_references: references,
                        is_anchor: anchors,
                        is_contact: vec![false; samples.len()],
                        gravity_m_per_s2: gravity,
                        characteristic_length_m: 0.10,
                        support_layout: if both_edges {
                            GravitySupportLayout::BothEdges
                        } else {
                            GravitySupportLayout::OneEdge
                        },
                        conformity,
                        supported_strip: vec![true; samples.len()],
                    };
                    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
                    gravity_moved |= debug.gravity_positions_m.iter().zip(&debug.bridge_positions_m)
                        .any(|(a, b)| a.distance(*b) > 1.0e-9);
                    conformity_moved |= debug.conformity_positions_m.iter().zip(&debug.gravity_positions_m)
                        .any(|(a, b)| a.distance(*b) > 1.0e-9);
                    let points: Vec<_> = (0..samples.len()).map(|i| serde_json::json!({
                        "stable_id": i,
                        "u": samples[i],
                        "v": 0.5,
                        "anchor": input.is_anchor[i],
                        "initial_position_m": vector(input.positions_m[i]),
                        "support": match debug.support[i] {
                            SupportKind::Anchor => "Anchor",
                            SupportKind::Unsupported => "Unsupported",
                        },
                        "bridge_position_m": vector(debug.bridge_positions_m[i]),
                        "gravity_position_m": vector(debug.gravity_positions_m[i]),
                        "conformity_position_m": vector(debug.conformity_positions_m[i]),
                        "collision_position_m": vector(debug.collision_positions_m[i]),
                        "final_position_m": vector(debug.final_result.positions_m[i]),
                        "surface_position_m": vector(debug.final_result.surface_positions_m[i]),
                        "surface_normal": vector(debug.final_result.surface_normals[i]),
                        "separation_m": debug.final_result.separation_m[i],
                        "contact": is_surface_contact(
                            debug.final_result.separation_m[i], settings.collision_tolerance_m)
                    })).collect();
                    cases.push(serde_json::json!({
                        "case_id": format!("GSR-007L-{}-{}-G{}-C{}",
                            fixture_kind,
                            if both_edges { "both" } else { "one" },
                            if gravity_enabled { 1 } else { 0 },
                            if conformity == 0.0 { "0" } else if conformity == 0.5 { "05" } else { "1" }),
                        "fixture_kind": fixture_kind,
                        "support_layout": if both_edges { "BothEdges" } else { "OneEdge" },
                        "gravity_m_per_s2": vector(gravity),
                        "conformity": conformity,
                        "points": points
                    }));
                }
            }
        }
    }
    assert_eq!(cases.len(), 36);
    assert!(gravity_moved, "nonzero gravity must move at least one point");
    assert!(conformity_moved, "nonzero conformity must move at least one point");
    let document = serde_json::json!({
        "schema": "GSR-007L-full-stage-v1",
        "quasi_static_gravity_scale": settings.quasi_static_gravity_scale,
        "characteristic_length_m": 0.10,
        "conformity_reach_m": settings.conformity_reach_m,
        "collision_tolerance_m": settings.collision_tolerance_m,
        "contact_epsilon_m": sansacloth_reference::CONTACT_EPSILON_M,
        "comparison_epsilon": 0.000001,
        "cases": cases
    });
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../validation/surface-response-result/basic-v1/GSR-007L-full-stage.json");
    let mut output = serde_json::to_string_pretty(&document).unwrap();
    output.push('\n');
    std::fs::write(&path, output).expect("write GSR-007L snapshot");
    println!("GSR-007L | CASES=36 | POINTS=324 | GRAVITY_MOVED={} | CONFORMITY_MOVED={} | SNAPSHOT={} | PASS",
        gravity_moved, conformity_moved, path.display());
}
