//! GSR-007I: explicit analytic Surface Query snapshot generator.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{AnalyticFixtureSurfaceQuery, SurfaceQuery};

fn vec3(v: DVec3) -> serde_json::Value {
    serde_json::json!([v.x, v.y, v.z])
}

#[test]
#[ignore = "generates committed GSR-007I surface-query snapshot"]
fn generate_gsr_007i_surface_query_snapshot() {
    let fixtures = [
        ("flat", AnalyticFixture::flat()),
        ("convex", AnalyticFixture::convex()),
        ("concave_shallow", AnalyticFixture::concave_shallow()),
        ("concave_deep", AnalyticFixture::concave_deep()),
    ];
    let us = [0.0, 0.20, 0.25, 0.375, 0.50, 0.75, 1.0];
    let vs = [0.25, 0.75];
    let offsets = [-0.01, 0.0, 0.02];
    let mut probes = Vec::new();
    for (kind, fixture) in fixtures {
        let query = AnalyticFixtureSurfaceQuery { fixture };
        for u in us {
            for v in vs {
                let reference = SurfaceReference::new(1, u, v).unwrap();
                let origin = query.query(DVec3::ZERO, reference);
                for offset in offsets {
                    let current = origin.surface_position_m + origin.surface_normal * offset;
                    let result = query.query(current, reference);
                    let stable_id = probes.len();
                    probes.push(serde_json::json!({
                        "stable_id": stable_id,
                        "fixture_kind": kind,
                        "u": u,
                        "v": v,
                        "offset_m": offset,
                        "current_position_m": vec3(current),
                        "surface_position_m": vec3(result.surface_position_m),
                        "surface_normal": vec3(result.surface_normal),
                        "separation_m": result.separation_m
                    }));
                }
            }
        }
    }
    assert_eq!(probes.len(), 168);
    let document = serde_json::json!({
        "schema": "GSR-007I-surface-query-v1",
        "tolerance": 0.000001,
        "probes": probes
    });
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../../validation/surface-response-result/basic-v1/GSR-007I-surface-query.json");
    let mut serialized = serde_json::to_string_pretty(&document).unwrap();
    serialized.push('\n');
    std::fs::write(&path, serialized).expect("write GSR-007I snapshot");
    println!("GSR-007I | SNAPSHOT={} | PROBES=168 | PASS", path.display());
}
