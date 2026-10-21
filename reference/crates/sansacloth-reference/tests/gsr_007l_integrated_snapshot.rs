//! Explicit GSR-007L curved full-stage Reference solver snapshot generator.
//! Run with --ignored; normal cargo test does not write files.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    is_surface_contact, AnalyticFixtureSurfaceQuery, GravitySupportLayout,
    ReferenceSolverSettings, ReferenceSurfaceSolver, ReferenceSurfaceSolverInput,
    SurfaceQuery, SupportKind,
};

fn vec3(v: DVec3) -> serde_json::Value {
    serde_json::json!([v.x, v.y, v.z])
}

#[test]
#[ignore = "writes GSR-007L snapshot; run explicitly"]
fn generate_gsr_007l_integrated_snapshot() {
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.1,
        conformity_reach_m: 0.02,
        collision_tolerance_m: 0.005,
    };
    let us = [0.0, 0.20, 0.25, 0.375, 0.5, 0.625, 0.75, 0.80, 1.0];
    let mut cases = Vec::new();
    let mut gravity_moved = false;
    let mut conformity_moved = false;

    for (kind, fixture) in [
        ("convex", AnalyticFixture::convex()),
        ("concave_shallow", AnalyticFixture::concave_shallow()),
        ("concave_deep", AnalyticFixture::concave_deep()),
    ] {
        let query = AnalyticFixtureSurfaceQuery { fixture };
        for both_edges in [true, false] {
            for gravity_enabled in [false, true] {
                for conformity in [0.0, 0.5, 1.0] {
                    let references: Vec<_> = us
                        .iter()
                        .map(|&u| SurfaceReference::new(1, u, 0.5).unwrap())
                        .collect();
                    let mut positions: Vec<DVec3> = references
                        .iter()
                        .map(|&r| {
                            let q = query.query(DVec3::ZERO, r);
                            q.surface_position_m + q.surface_normal * 0.015
                        })
                        .collect();
                    // A displaced midpoint distinguishes Bridge from the one-edge path.
                    positions[4] -= query.query(DVec3::ZERO, references[4]).surface_normal * 0.025;
                    let is_anchor: Vec<bool> = (0..us.len())
                        .map(|i| i == 0 || (both_edges && i == us.len() - 1))
                        .collect();
                    let gravity = if gravity_enabled {
                        DVec3::new(0.0, -9.80665, 0.0)
                    } else {
                        DVec3::ZERO
                    };
                    let input = ReferenceSurfaceSolverInput {
                        positions_m: positions,
                        surface_references: references,
                        is_anchor,
                        is_contact: vec![false; us.len()],
                        gravity_m_per_s2: gravity,
                        characteristic_length_m: 0.10,
                        support_layout: if both_edges {
                            GravitySupportLayout::BothEdges
                        } else {
                            GravitySupportLayout::OneEdge
                        },
                        conformity,
                        supported_strip: vec![true; us.len()],
                    };
                    let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, settings);
                    gravity_moved |= debug.gravity_positions_m.iter().zip(&debug.bridge_positions_m)
                        .any(|(a, b)| a.distance(*b) > 1.0e-9);
                    conformity_moved |= debug.conformity_positions_m.iter().zip(&debug.gravity_positions_m)
                        .any(|(a, b)| a.distance(*b) > 1.0e-9);
                    let points: Vec<_> = (0..us.len()).map(|i| {
                        serde_json::json!({
                            "stable_id": i,
                            "u": us[i], "v": 0.5,
                            "anchor": input.is_anchor[i],
                            "supported_strip": input.supported_strip[i],
                            "initial_position_m": vec3(input.positions_m[i]),
                            "support": match debug.support[i] {
                                SupportKind::Anchor => "Anchor",
                                SupportKind::Unsupported => "Unsupported",
                            },
                            "bridge_position_m": vec3(debug.bridge_positions_m[i]),
                            "gravity_position_m": vec3(debug.gravity_positions_m[i]),
                            "conformity_position_m": vec3(debug.conformity_positions_m[i]),
                            "collision_position_m": vec3(debug.collision_positions_m[i]),
                            "final_position_m": vec3(debug.final_result.positions_m[i]),
                            "surface_position_m": vec3(debug.final_result.surface_positions_m[i]),
                            "surface_normal": vec3(debug.final_result.surface_normals[i]),
                            "separation_m": debug.final_result.separation_m[i],
                            "contact": is_surface_contact(
                                debug.final_result.separation_m[i],
                                settings.collision_tolerance_m
                            ),
                        })
                    }).collect();
                    let case_id = format!(
                        "GSR-007L-{}-{}-G{}-C{}",
                        kind,
                        if both_edges { "both" } else { "one" },
                        if gravity_enabled { 1 } else { 0 },
                        if conformity == 0.0 { 0 } else if conformity == 0.5 { 5 } else { 10 }
                    );
                    cases.push(serde_json::json!({
                        "case_id": case_id,
                        "fixture_kind": kind,
                        "support_layout": if both_edges { "BothEdges" } else { "OneEdge" },
                        "gravity_m_per_s2": vec3(gravity),
                        "conformity": conformity,
                        "points": points,
                    }));
                }
            }
        }
    }
    assert_eq!(cases.len(), 36);
    assert!(gravity_moved, "Gravity must produce a nonzero displacement");
    assert!(conformity_moved, "Conformity must produce a nonzero displacement");

    let snapshot = serde_json::json!({
        "schema": "GSR-007L-integrated-v1",
        "quasi_static_gravity_scale": settings.quasi_static_gravity_scale,
        "characteristic_length_m": 0.10,
        "conformity_reach_m": settings.conformity_reach_m,
        "collision_tolerance_m": settings.collision_tolerance_m,
        "contact_epsilon_m": sansacloth_reference::CONTACT_EPSILON_M,
        "comparison_epsilon": 0.000001,
        "cases": cases,
    });
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../validation/surface-response-result/basic-v1/GSR-007L-integrated.json");
    let mut json = serde_json::to_string_pretty(&snapshot).unwrap();
    json.push('\n');
    std::fs::write(&path, json).expect("write GSR-007L snapshot");
    println!(
        "GSR-007L | CASES=36 | POINTS=324 | GRAVITY_MOVED={} | CONFORMITY_MOVED={} | SNAPSHOT={} | PASS",
        gravity_moved, conformity_moved, path.display()
    );
}
