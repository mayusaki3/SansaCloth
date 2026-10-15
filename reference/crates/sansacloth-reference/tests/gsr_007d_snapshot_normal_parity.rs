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

    let fixture_json = include_str!("../../../validation/fixture-exchange/basic-v1/SR-004-C1-G0.json");
    let fixture_document: Value = serde_json::from_str(fixture_json).expect("valid fixture exchange");
    let vertices = fixture_document["body_surface"]["vertices"]
        .as_array().expect("body surface vertices");
    assert_eq!(vertices.len(), points.len());
    let mut fixture_mismatches = Vec::new();
    let mut surface_position_mismatches = Vec::new();

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
        let query_result = query.query(DVec3::ZERO, surface_reference);
        let actual = query_result.surface_normal;
        let saved_position = point["surface_position_m"].as_array().expect("saved surface position");
        let expected_position = DVec3::new(
            saved_position[0].as_f64().unwrap(),
            saved_position[1].as_f64().unwrap(),
            saved_position[2].as_f64().unwrap(),
        );
        let position_error = query_result.surface_position_m.distance(expected_position);
        if position_error > 1.0e-6 {
            surface_position_mismatches.push((stable_id, position_error));
        }
        let saved = point["surface_normal"].as_array().expect("saved normal");
        let expected = DVec3::new(
            saved[0].as_f64().unwrap(),
            saved[1].as_f64().unwrap(),
            saved[2].as_f64().unwrap(),
        );
        let vertex = &vertices[index];
        let fixture_normal = &vertex["normal"];
        let fixture_value = DVec3::new(
            fixture_normal[0].as_f64().unwrap(),
            fixture_normal[1].as_f64().unwrap(),
            fixture_normal[2].as_f64().unwrap(),
        );
        let fixture_error = actual.distance(fixture_value);
        if fixture_error > 1.0e-6 {
            fixture_mismatches.push((stable_id, fixture_error));
        }
        let error = actual.distance(expected);
        if error > 1.0e-6 {
            mismatches.push((stable_id, error));
        }
    }
    assert!(
        surface_position_mismatches.is_empty(),
        "committed SR-004 surface positions differ from current analytic Rust query: {:?}; total: {}",
        &surface_position_mismatches[..surface_position_mismatches.len().min(5)],
        surface_position_mismatches.len()
    );
    assert!(
        fixture_mismatches.is_empty(),
        "committed fixture-exchange normals differ from current analytic Rust query: {:?}; total: {}",
        &fixture_mismatches[..fixture_mismatches.len().min(5)],
        fixture_mismatches.len()
    );
    assert!(
        mismatches.is_empty(),
        "committed SR-004 normals differ from current analytic Rust query; first mismatches: {:?}; total: {}",
        &mismatches[..mismatches.len().min(5)],
        mismatches.len()
    );
}


/// Diagnostic only: identify whether the committed result normals were
/// generated from a backward finite difference of sampled surface positions.
/// This does not make those normals the Reference solver contract.
#[test]
fn gsr_007d_identify_saved_normal_derivation() {
    let json = include_str!("../../../validation/surface-response-result/basic-v1/SR-004-C1-G0.json");
    let document: Value = serde_json::from_str(json).expect("valid result snapshot");
    let points = document["control_points"].as_array().expect("control points");
    let mut chord_matches = 0usize;
    let mut analytic_mismatches = 0usize;
    let fixture = AnalyticFixture::concave_shallow();
    for index in 0..points.len() {
        let u_index = index % 21;
        if u_index == 0 { continue; }
        let current = &points[index]["surface_position_m"];
        let previous = &points[index - 1]["surface_position_m"];
        let dx = current[0].as_f64().unwrap() - previous[0].as_f64().unwrap();
        let dy = current[1].as_f64().unwrap() - previous[1].as_f64().unwrap();
        let chord = DVec3::new(-dy, dx, 0.0).normalize();
        let saved = &points[index]["surface_normal"];
        let saved_normal = DVec3::new(
            saved[0].as_f64().unwrap(),
            saved[1].as_f64().unwrap(),
            saved[2].as_f64().unwrap(),
        );
        let u = u_index as f64 / 20.0;
        if saved_normal.distance(chord) < 1.0e-6 {
            chord_matches += 1;
        }
        if saved_normal.distance(fixture.normal(u)) > 1.0e-6 {
            analytic_mismatches += 1;
        }
    }
    println!(
        "GSR-007D diagnostic: backward_chord_matches={chord_matches}/140 analytic_mismatches={analytic_mismatches}/140"
    );
    assert_eq!(chord_matches, 140, "saved normals are not uniformly backward-chord normals");
    assert_eq!(analytic_mismatches, 70, "unexpected analytic mismatch count");
}
