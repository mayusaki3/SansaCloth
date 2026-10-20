//! Explicit GSR-007K curved analytic Surface Query → Collision snapshot generator.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    apply_collision_response, is_surface_contact, AnalyticFixtureSurfaceQuery,
    CollisionResponseInput, ReferenceSolverSettings, SurfaceQuery,
};

fn vector(v: DVec3) -> serde_json::Value {
    serde_json::json!([v.x, v.y, v.z])
}

#[test]
#[ignore = "writes GSR-007K reference snapshot explicitly"]
fn generate_gsr_007k_snapshot() {
    let fixtures = [
        ("convex", AnalyticFixture::convex()),
        ("concave_shallow", AnalyticFixture::concave_shallow()),
        ("concave_deep", AnalyticFixture::concave_deep()),
    ];
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: 0.0,
        conformity_reach_m: 0.02,
        collision_tolerance_m: 0.005,
    };
    let mut probes = Vec::new();
    for (kind, fixture) in fixtures {
        let query = AnalyticFixtureSurfaceQuery { fixture };
        for u in [0.0, 0.20, 0.25, 0.375, 0.5, 0.625, 0.75, 0.80, 1.0] {
            let v = 0.5;
            let reference = SurfaceReference::new(1, u, v).unwrap();
            let base = query.query(DVec3::ZERO, reference);
            for offset in [-0.01, 0.005, 0.02] {
                let initial = base.surface_position_m + base.surface_normal * offset;
                let before = query.query(initial, reference);
                let corrected = apply_collision_response(
                    &CollisionResponseInput {
                        positions_m: vec![initial],
                        separation_m: vec![before.separation_m],
                        surface_normals: vec![before.surface_normal],
                    },
                    settings,
                )[0];
                let after = query.query(corrected, reference);
                probes.push(serde_json::json!({
                    "stable_id": probes.len(),
                    "fixture_kind": kind,
                    "u": u, "v": v, "offset_m": offset,
                    "initial_position_m": vector(initial),
                    "before_surface_position_m": vector(before.surface_position_m),
                    "before_surface_normal": vector(before.surface_normal),
                    "before_separation_m": before.separation_m,
                    "collision_position_m": vector(corrected),
                    "after_surface_position_m": vector(after.surface_position_m),
                    "after_surface_normal": vector(after.surface_normal),
                    "after_separation_m": after.separation_m,
                    "contact": is_surface_contact(after.separation_m, settings.collision_tolerance_m)
                }));
            }
        }
    }
    assert_eq!(probes.len(), 81);
    let document = serde_json::json!({
        "schema": "GSR-007K-curved-collision-v1",
        "collision_tolerance_m": settings.collision_tolerance_m,
        "contact_epsilon_m": sansacloth_reference::CONTACT_EPSILON_M,
        "comparison_epsilon": 0.000001,
        "probes": probes
    });
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../validation/surface-response-result/basic-v1/GSR-007K-curved-collision.json");
    let mut output = serde_json::to_string_pretty(&document).unwrap();
    output.push('\n');
    std::fs::write(&path, output).expect("write GSR-007K snapshot");
    println!("GSR-007K | PROBES=81 | SNAPSHOT={} | PASS", path.display());
}
