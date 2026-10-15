//! Regression guard: committed SurfaceResponse snapshots must agree with the
//! current analytic Reference surface query, not only with a Godot snapshot reader.
//! This is intentionally a RED test until the snapshot generation path is reconciled.
use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{AnalyticFixtureSurfaceQuery, SurfaceQuery};
use serde_json::Value;

#[test]
fn gsr_007d_sr004_committed_normals_match_current_rust_reference() {
    let json = include_str!("../../../validation/surface-response-result/basic-v1/SR-004-C1-G0.json");
    let document: Value = serde_json::from_str(json).expect("valid reference snapshot");
    assert_eq!(document["case_id"].as_str(), Some("SR-004-C1-G0"));
    let points = document["control_points"].as_array().expect("control points");
    assert_eq!(points.len(), 147);

    let query = AnalyticFixtureSurfaceQuery {
        fixture: AnalyticFixture::concave_shallow(),
    };
    let mut mismatches = Vec::new();
    for (index, point) in points.iter().enumerate() {
        let stable_id = point["stable_id"].as_u64().expect("stable id");
        // The basic exchange fixture is a 21x7 grid, U-major within each V row.
        assert_eq!(stable_id as usize, index);
        let u = (index % 21) as f64 / 20.0;
        let v = (index / 21) as f64 / 6.0;
        let surface_reference = SurfaceReference::new(1, u, v).unwrap();
        let actual = query.query(DVec3::ZERO, surface_reference).surface_normal;
        let saved = point["surface_normal"].as_array().expect("saved normal");
        let expected = DVec3::new(
            saved[0].as_f64().unwrap(),
            saved[1].as_f64().unwrap(),
            saved[2].as_f64().unwrap(),
        );
        let error = actual.distance(expected);
        if error > 1.0e-6 {
            mismatches.push((stable_id, error));
        }
    }
    assert!(
        mismatches.is_empty(),
        "committed SR-004 normals differ from current analytic Rust query; first mismatches: {:?}; total: {}",
        &mismatches[..mismatches.len().min(5)],
        mismatches.len()
    );
}
