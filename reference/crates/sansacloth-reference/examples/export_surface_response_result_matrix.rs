use std::{
    collections::BTreeMap,
    env, fs,
    path::{Path, PathBuf},
};

use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::exchange::{
    BASIC_EXCHANGE_CASE_COUNT, ExchangeBodySurface, FixtureExchange, generate_basic_exchange_matrix,
};
use sansacloth_reference::{
    GravitySupportLayout, ReferenceSolverSettings, ReferenceSurfaceSolver,
    ReferenceSurfaceSolverInput, SupportKind, SurfaceQuery, SurfaceQueryResult,
};
use serde::Serialize;

const RESULT_FORMAT: &str = "sansacloth.validation.surface-response-result/0";
const CHARACTERISTIC_LENGTH_M: f64 = 0.10;
const QUASI_STATIC_GRAVITY_SCALE: f64 = 0.1;
const CONFORMITY_REACH_M: f64 = 0.02;

#[derive(Serialize)]
struct ValidationResult {
    format: &'static str,
    case_id: String,
    profile: ValidationProfile,
    control_points: Vec<ControlPointResult>,
    aggregate: AggregateResult,
}

#[derive(Serialize)]
struct ValidationProfile {
    characteristic_length_m: f64,
    quasi_static_gravity_scale: f64,
    conformity_reach_m: f64,
    support_layout: &'static str,
}

#[derive(Serialize)]
struct ControlPointResult {
    stable_id: u64,
    support: &'static str,
    bridge_position_m: [f64; 3],
    gravity_position_m: [f64; 3],
    conformity_position_m: [f64; 3],
    collision_position_m: [f64; 3],
    final_position_m: [f64; 3],
    surface_position_m: [f64; 3],
    surface_normal: [f64; 3],
    separation_m: f64,
    contact: bool,
}

#[derive(Serialize)]
struct AggregateResult {
    control_point_count: usize,
    support_count: usize,
    contact_count: usize,
}

struct ResolvedExchangeSurfaceQuery<'a> {
    surface: &'a ExchangeBodySurface,
}

impl SurfaceQuery for ResolvedExchangeSurfaceQuery<'_> {
    fn query(
        &self,
        current_position_m: DVec3,
        surface_reference: SurfaceReference,
    ) -> SurfaceQueryResult {
        assert_eq!(surface_reference.domain_id, self.surface.domain_id);
        let p = [surface_reference.u, surface_reference.v];

        for triangle in &self.surface.triangles {
            let v0 = &self.surface.vertices[triangle[0]];
            let v1 = &self.surface.vertices[triangle[1]];
            let v2 = &self.surface.vertices[triangle[2]];
            let Some(weights) = barycentric(p, v0.uv, v1.uv, v2.uv) else {
                continue;
            };

            let p0 = vec3(v0.position_m);
            let p1 = vec3(v1.position_m);
            let p2 = vec3(v2.position_m);
            let surface_position_m = p0 * weights[0] + p1 * weights[1] + p2 * weights[2];
            let surface_normal = (p1 - p0).cross(p2 - p0).normalize();
            let separation_m = (current_position_m - surface_position_m).dot(surface_normal);
            return SurfaceQueryResult {
                surface_position_m,
                surface_normal,
                separation_m,
            };
        }

        panic!(
            "SurfaceReference ({},{}) was not resolved in domain {}",
            surface_reference.u, surface_reference.v, surface_reference.domain_id
        );
    }
}

#[derive(Serialize)]
struct CollisionDiagnostic {
    applied: bool,
    input_position_m: [f64; 3],
    surface_position_m: [f64; 3],
    surface_normal: [f64; 3],
    separation_m: f64,
    normalized_normal: [f64; 3],
    correction_m: f64,
    offset_m: [f64; 3],
}

#[derive(Serialize)]
struct QueryDiagnostic {
    stable_id: u64,
    collision_diagnostic: CollisionDiagnostic,
    triangle_positions_m: [[f64; 3]; 3],
    edge_01_m: [f64; 3],
    edge_02_m: [f64; 3],
    json_roundtrip_triangle_positions_m: [[f64; 3]; 3],
    barycentric_weights: [f64; 3],
    raw_normal: [f64; 3],
    normal_length: f64,
    delta_position_m: [f64; 3],
    separation_dot_terms: [f64; 3],
}

fn diagnostic_query(
    surface: &ExchangeBodySurface,
    position: DVec3,
    reference: SurfaceReference,
    stable_id: u64,
    collision_diagnostic: CollisionDiagnostic,
) -> QueryDiagnostic {
    let uv = [reference.u, reference.v];
    for triangle in &surface.triangles {
        let v0 = &surface.vertices[triangle[0]];
        let v1 = &surface.vertices[triangle[1]];
        let v2 = &surface.vertices[triangle[2]];
        if let Some(weights) = barycentric(uv, v0.uv, v1.uv, v2.uv) {
            let p0 = vec3(v0.position_m);
            let p1 = vec3(v1.position_m);
            let p2 = vec3(v2.position_m);
            // Mirror the JSON number serialization/parsing boundary used by O3DE.
            let roundtrip = [v0.position_m, v1.position_m, v2.position_m]
                .map(|v| v.map(|x| serde_json::from_str::<f64>(&serde_json::to_string(&x).unwrap()).unwrap()));
            let surface_position = p0 * weights[0] + p1 * weights[1] + p2 * weights[2];
            let raw = (p1 - p0).cross(p2 - p0);
            let normal = raw.normalize();
            let delta = position - surface_position;
            return QueryDiagnostic {
                stable_id,
                collision_diagnostic,
                triangle_positions_m: [array3(p0), array3(p1), array3(p2)],
                edge_01_m: array3(p1 - p0),
                edge_02_m: array3(p2 - p0),
                json_roundtrip_triangle_positions_m: roundtrip,
                barycentric_weights: weights,
                raw_normal: array3(raw),
                normal_length: raw.length(),
                delta_position_m: array3(delta),
                separation_dot_terms: [delta.x * normal.x, delta.y * normal.y, delta.z * normal.z],
            };
        }
    }
    panic!("unresolved diagnostic surface reference for {stable_id}");
}

fn write_query_diagnostics(
    output_dir: &Path,
    exchange: &FixtureExchange,
    result: &ValidationResult,
) -> Result<(), Box<dyn std::error::Error>> {
    let by_id: BTreeMap<_, _> = exchange.cloth.control_points.iter()
        .map(|p| (p.stable_id, p)).collect();
    let mut diagnostics = Vec::with_capacity(result.control_points.len());
    for point in &result.control_points {
        let source = by_id[&point.stable_id];
        let reference = SurfaceReference::new(
            source.surface_reference.domain_id,
            source.surface_reference.u,
            source.surface_reference.v,
        ).unwrap();
        let conformity_position = vec3(point.conformity_position_m);
        let collision_query = ResolvedExchangeSurfaceQuery {
            surface: &exchange.body_surface,
        }.query(conformity_position, reference);
        let tolerance = exchange.inputs.collision_tolerance_m;
        let applied = collision_query.separation_m < tolerance;
        let normalized_normal = if applied {
            collision_query.surface_normal.try_normalize()
                .expect("collision correction requires a finite non-zero surface normal")
        } else {
            DVec3::ZERO
        };
        let correction_m = if applied { tolerance - collision_query.separation_m } else { 0.0 };
        let offset = normalized_normal * correction_m;
        let collision_diagnostic = CollisionDiagnostic {
            applied,
            input_position_m: point.conformity_position_m,
            surface_position_m: array3(collision_query.surface_position_m),
            surface_normal: array3(collision_query.surface_normal),
            separation_m: collision_query.separation_m,
            normalized_normal: array3(normalized_normal),
            correction_m,
            offset_m: array3(offset),
        };
        diagnostics.push(diagnostic_query(
            &exchange.body_surface,
            vec3(point.final_position_m),
            reference,
            point.stable_id,
            collision_diagnostic,
        ));
    }
    let path = output_dir.join(format!("{}.query-diagnostic.json", exchange.case_id));
    fs::write(path, serde_json::to_vec_pretty(&diagnostics)?)?;
    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let output_dir = env::args_os().nth(1).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from("validation")
            .join("surface-response-result")
            .join("basic-v1")
    });
    fs::create_dir_all(&output_dir)?;

    let exchanges = generate_basic_exchange_matrix();
    if exchanges.len() != BASIC_EXCHANGE_CASE_COUNT {
        return Err(format!(
            "expected {BASIC_EXCHANGE_CASE_COUNT} cases, got {}",
            exchanges.len()
        )
        .into());
    }

    // Optional comparison run: solve exactly the Fixture Exchange values after JSON roundtrip.
    // Keep the canonical Reference outputs untouched.
    let roundtrip_dir = env::var_os("SANSA_SURFACE_JSON_ROUNDTRIP_RESULT_DIR").map(PathBuf::from);
    if let Some(dir) = &roundtrip_dir { fs::create_dir_all(dir)?; }
    let diagnostic_dir = env::var_os("SANSA_SURFACE_QUERY_DIAGNOSTIC_DIR").map(PathBuf::from);
    if let Some(dir) = &diagnostic_dir { fs::create_dir_all(dir)?; }
    let mut written = 0usize;
    for exchange in &exchanges {
        let result = solve_exchange(exchange);
        write_result(&output_dir, &result)?;
        if let Some(dir) = &roundtrip_dir {
            let serialized = serde_json::to_vec(exchange)?;
            let decoded: FixtureExchange = serde_json::from_slice(&serialized)?;
            let roundtrip_result = solve_exchange(&decoded);
            write_result(dir, &roundtrip_result)?;
            if let Some(diagnostic_dir) = &diagnostic_dir {
                let roundtrip_diagnostic_dir = diagnostic_dir.join("json-roundtrip");
                fs::create_dir_all(&roundtrip_diagnostic_dir)?;
                write_query_diagnostics(&roundtrip_diagnostic_dir, &decoded, &roundtrip_result)?;
            }
        }
        if let Some(dir) = &diagnostic_dir { write_query_diagnostics(dir, exchange, &result)?; }
        println!("SANSA_FXE|FXE-017A.CASE.{}|PASS", exchange.case_id);
        written += 1;
    }

    println!("SANSA_FXE|FXE-017A.CASE_COUNT|{}", exchanges.len());
    println!("SANSA_FXE|FXE-017A.WRITTEN_COUNT|{written}");
    println!("SANSA_FXE|FXE-017A.OUTPUT_DIR|{}", output_dir.display());
    println!("SANSA_FXE|FXE-017A.RESULT|PASS");
    Ok(())
}

fn solve_exchange(exchange: &FixtureExchange) -> ValidationResult {
    let query = ResolvedExchangeSurfaceQuery {
        surface: &exchange.body_surface,
    };
    let support_layout = if exchange.case_id.starts_with("SR-003-") {
        GravitySupportLayout::OneEdge
    } else {
        GravitySupportLayout::BothEdges
    };
    let support_layout_name = match support_layout {
        GravitySupportLayout::BothEdges => "BothEdges",
        GravitySupportLayout::OneEdge => "OneEdge",
    };
    let settings = ReferenceSolverSettings {
        quasi_static_gravity_scale: QUASI_STATIC_GRAVITY_SCALE,
        conformity_reach_m: CONFORMITY_REACH_M,
        collision_tolerance_m: exchange.inputs.collision_tolerance_m,
    };
    let gravity = vec3(exchange.inputs.world_gravity_m_per_s2);

    let mut strips: BTreeMap<u64, Vec<_>> = BTreeMap::new();
    for point in &exchange.cloth.control_points {
        strips.entry(point.strip_id).or_default().push(point);
    }
    for points in strips.values_mut() {
        points.sort_by_key(|point| point.strip_order);
    }

    let mut by_stable_id = BTreeMap::new();
    for points in strips.values() {
        let positions_m: Vec<_> = points.iter().map(|point| vec3(point.position_m)).collect();
        let surface_references: Vec<_> = points
            .iter()
            .map(|point| {
                SurfaceReference::new(
                    point.surface_reference.domain_id,
                    point.surface_reference.u,
                    point.surface_reference.v,
                )
                .unwrap()
            })
            .collect();
        let debug = ReferenceSurfaceSolver::solve_with_debug(
            &ReferenceSurfaceSolverInput {
                positions_m,
                surface_references,
                is_anchor: points.iter().map(|point| point.anchor).collect(),
                is_contact: points.iter().map(|point| point.contact).collect(),
                gravity_m_per_s2: gravity,
                characteristic_length_m: CHARACTERISTIC_LENGTH_M,
                support_layout,
                conformity: exchange.inputs.conformity,
                supported_strip: vec![true; points.len()],
            },
            &query,
            settings,
        );

        for (index, point) in points.iter().enumerate() {
            let final_result = &debug.final_result;
            let separation_m = final_result.separation_m[index];
            by_stable_id.insert(
                point.stable_id,
                ControlPointResult {
                    stable_id: point.stable_id,
                    support: support_name(final_result.support[index]),
                    bridge_position_m: array3(debug.bridge_positions_m[index]),
                    gravity_position_m: array3(debug.gravity_positions_m[index]),
                    conformity_position_m: array3(debug.conformity_positions_m[index]),
                    collision_position_m: array3(debug.collision_positions_m[index]),
                    final_position_m: array3(final_result.positions_m[index]),
                    surface_position_m: array3(final_result.surface_positions_m[index]),
                    surface_normal: array3(final_result.surface_normals[index]),
                    separation_m,
                    contact: separation_m <= settings.collision_tolerance_m,
                },
            );
        }
    }

    let control_points: Vec<_> = by_stable_id.into_values().collect();
    let support_count = control_points
        .iter()
        .filter(|point| point.support != "Unsupported")
        .count();
    let contact_count = control_points.iter().filter(|point| point.contact).count();

    ValidationResult {
        format: RESULT_FORMAT,
        case_id: exchange.case_id.clone(),
        profile: ValidationProfile {
            characteristic_length_m: CHARACTERISTIC_LENGTH_M,
            quasi_static_gravity_scale: QUASI_STATIC_GRAVITY_SCALE,
            conformity_reach_m: CONFORMITY_REACH_M,
            support_layout: support_layout_name,
        },
        aggregate: AggregateResult {
            control_point_count: control_points.len(),
            support_count,
            contact_count,
        },
        control_points,
    }
}

fn write_result(
    output_dir: &Path,
    result: &ValidationResult,
) -> Result<(), Box<dyn std::error::Error>> {
    let path = output_dir.join(format!("{}.json", result.case_id));
    let json = serde_json::to_string_pretty(result)?;
    fs::write(path, format!("{json}\n"))?;
    Ok(())
}

fn barycentric(p: [f64; 2], a: [f64; 2], b: [f64; 2], c: [f64; 2]) -> Option<[f64; 3]> {
    let v0 = [b[0] - a[0], b[1] - a[1]];
    let v1 = [c[0] - a[0], c[1] - a[1]];
    let v2 = [p[0] - a[0], p[1] - a[1]];
    let denominator = v0[0] * v1[1] - v1[0] * v0[1];
    if denominator.abs() <= 1.0e-15 {
        return None;
    }
    let w1 = (v2[0] * v1[1] - v1[0] * v2[1]) / denominator;
    let w2 = (v0[0] * v2[1] - v2[0] * v0[1]) / denominator;
    let w0 = 1.0 - w1 - w2;
    let epsilon = 1.0e-12;
    if w0 >= -epsilon && w1 >= -epsilon && w2 >= -epsilon {
        Some([w0, w1, w2])
    } else {
        None
    }
}

fn vec3(value: [f64; 3]) -> DVec3 {
    DVec3::new(value[0], value[1], value[2])
}

fn array3(value: DVec3) -> [f64; 3] {
    [value.x, value.y, value.z]
}

fn support_name(value: SupportKind) -> &'static str {
    match value {
        SupportKind::Anchor => "Anchor",
        SupportKind::Unsupported => "Unsupported",
    }
}
