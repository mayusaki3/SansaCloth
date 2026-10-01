//! Validation-only JSON exchange between the Reference fixtures and runtime backend probes.
//!
//! This module deliberately exchanges resolved validation data rather than Reference solver
//! algorithm settings. It is not a product serialization format.

use std::{
    collections::HashSet,
    error::Error,
    f64::consts::FRAC_PI_2,
    fmt::{self, Display},
};

use glam::{DQuat, DVec3};
use sansacloth_core::SurfaceReference;
use serde::{Deserialize, Serialize};

use crate::{AnalyticFixture, AnchorProfile, FlatClothFixture};

pub const FIXTURE_EXCHANGE_FORMAT: &str = "sansacloth.validation.fixture-exchange/0";
pub const BASIC_U_SAMPLES: usize = 21;
pub const BASIC_V_SAMPLES: usize = 7;
pub const BASIC_DOMAIN_ID: u64 = 1;
pub const BASIC_COLLISION_TOLERANCE_M: f64 = 0.0;
pub const BASIC_GRAVITY_M_PER_S2: [f64; 3] = [0.0, -9.80665, 0.0];

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct FixtureExchange {
    pub format: String,
    pub case_id: String,
    pub coordinate: ExchangeCoordinate,
    pub body_surface: ExchangeBodySurface,
    pub cloth: ExchangeCloth,
    pub inputs: ExchangeInputs,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeCoordinate {
    pub length_unit: String,
    pub gravity_unit: String,
    pub x: String,
    pub y: String,
    pub z: String,
}

impl Default for ExchangeCoordinate {
    fn default() -> Self {
        Self {
            length_unit: "m".into(),
            gravity_unit: "m/s^2".into(),
            x: "right".into(),
            y: "up".into(),
            z: "forward".into(),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeBodySurface {
    pub domain_id: u64,
    pub vertices: Vec<ExchangeBodyVertex>,
    pub triangles: Vec<[usize; 3]>,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeBodyVertex {
    pub position_m: [f64; 3],
    pub normal: [f64; 3],
    pub uv: [f64; 2],
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeCloth {
    pub control_points: Vec<ExchangeControlPoint>,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeControlPoint {
    pub stable_id: u64,
    pub strip_id: u64,
    pub strip_order: u64,
    pub position_m: [f64; 3],
    pub surface_reference: ExchangeSurfaceReference,
    pub anchor: bool,
    pub contact: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeSurfaceReference {
    pub domain_id: u64,
    pub u: f64,
    pub v: f64,
}

impl From<SurfaceReference> for ExchangeSurfaceReference {
    fn from(value: SurfaceReference) -> Self {
        Self {
            domain_id: value.domain_id,
            u: value.u,
            v: value.v,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExchangeInputs {
    pub world_gravity_m_per_s2: [f64; 3],
    pub conformity: f64,
    pub collision_tolerance_m: f64,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BasicExchangeScenario {
    Sr001Flat,
    Sr002ConvexUp,
    Sr003ConvexSide,
    Sr004ConcaveShallow,
    Sr005ConcaveDeep,
}

impl BasicExchangeScenario {
    fn id(self) -> &'static str {
        match self {
            Self::Sr001Flat => "SR-001",
            Self::Sr002ConvexUp => "SR-002",
            Self::Sr003ConvexSide => "SR-003",
            Self::Sr004ConcaveShallow => "SR-004",
            Self::Sr005ConcaveDeep => "SR-005",
        }
    }

    fn fixture(self) -> AnalyticFixture {
        match self {
            Self::Sr001Flat => AnalyticFixture::flat(),
            Self::Sr002ConvexUp | Self::Sr003ConvexSide => AnalyticFixture::convex(),
            Self::Sr004ConcaveShallow => AnalyticFixture::concave_shallow(),
            Self::Sr005ConcaveDeep => AnalyticFixture::concave_deep(),
        }
    }

    fn anchor_profile(self) -> AnchorProfile {
        match self {
            Self::Sr003ConvexSide => AnchorProfile::EdgeU0,
            _ => AnchorProfile::BothEdgesU,
        }
    }

    fn rotation(self) -> DQuat {
        match self {
            Self::Sr003ConvexSide => DQuat::from_rotation_z(-FRAC_PI_2),
            _ => DQuat::IDENTITY,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BasicGravityCase {
    Off,
    On,
}

impl BasicGravityCase {
    fn id(self) -> &'static str {
        match self {
            Self::Off => "G0",
            Self::On => "G1",
        }
    }

    fn vector(self) -> [f64; 3] {
        match self {
            Self::Off => [0.0; 3],
            Self::On => BASIC_GRAVITY_M_PER_S2,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum BasicConformityCase {
    C0,
    C05,
    C1,
}

impl BasicConformityCase {
    fn id(self) -> &'static str {
        match self {
            Self::C0 => "C0",
            Self::C05 => "C05",
            Self::C1 => "C1",
        }
    }

    fn value(self) -> f64 {
        match self {
            Self::C0 => 0.0,
            Self::C05 => 0.5,
            Self::C1 => 1.0,
        }
    }
}

#[derive(Debug)]
pub enum FixtureExchangeError {
    Json(serde_json::Error),
    Validation(String),
}

impl Display for FixtureExchangeError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Json(error) => write!(formatter, "fixture exchange JSON error: {error}"),
            Self::Validation(message) => {
                write!(formatter, "fixture exchange validation error: {message}")
            }
        }
    }
}

impl Error for FixtureExchangeError {
    fn source(&self) -> Option<&(dyn Error + 'static)> {
        match self {
            Self::Json(error) => Some(error),
            Self::Validation(_) => None,
        }
    }
}

impl From<serde_json::Error> for FixtureExchangeError {
    fn from(value: serde_json::Error) -> Self {
        Self::Json(value)
    }
}

impl FixtureExchange {
    pub fn to_json_pretty(&self) -> Result<String, FixtureExchangeError> {
        self.validate()?;
        Ok(serde_json::to_string_pretty(self)?)
    }

    pub fn from_json(json: &str) -> Result<Self, FixtureExchangeError> {
        let exchange: Self = serde_json::from_str(json)?;
        exchange.validate()?;
        Ok(exchange)
    }

    pub fn validate(&self) -> Result<(), FixtureExchangeError> {
        validate_exchange(self)
    }
}

pub fn generate_basic_exchange(
    scenario: BasicExchangeScenario,
    gravity: BasicGravityCase,
    conformity: BasicConformityCase,
) -> FixtureExchange {
    let fixture = scenario.fixture();
    let rotation = scenario.rotation();
    let anchor_profile = scenario.anchor_profile();
    let mesh = fixture.sample_mesh(BASIC_U_SAMPLES, BASIC_V_SAMPLES);

    let mut vertices = Vec::with_capacity(mesh.positions.len());
    for j in 0..BASIC_V_SAMPLES {
        let v = j as f64 / (BASIC_V_SAMPLES - 1) as f64;
        for i in 0..BASIC_U_SAMPLES {
            let u = i as f64 / (BASIC_U_SAMPLES - 1) as f64;
            let index = j * BASIC_U_SAMPLES + i;
            vertices.push(ExchangeBodyVertex {
                position_m: vec3_array(rotation * mesh.positions[index]),
                normal: vec3_array((rotation * fixture.normal(u)).normalize()),
                uv: [u, v],
            });
        }
    }

    let cloth_fixture = FlatClothFixture;
    let control_points = cloth_fixture.control_points(BASIC_U_SAMPLES, BASIC_V_SAMPLES);
    let control_points = control_points
        .iter()
        .enumerate()
        .map(|(index, point)| {
            let position_m = rotation * point.position_m;
            let surface_reference =
                SurfaceReference::new(BASIC_DOMAIN_ID, point.u, point.v).unwrap();
            let surface_position_m = rotation * fixture.position(point.u, point.v);
            let surface_normal = (rotation * fixture.normal(point.u)).normalize();
            let separation_m = (position_m - surface_position_m).dot(surface_normal);

            ExchangeControlPoint {
                stable_id: point.stable_id,
                strip_id: (index / BASIC_U_SAMPLES) as u64,
                strip_order: (index % BASIC_U_SAMPLES) as u64,
                position_m: vec3_array(position_m),
                surface_reference: surface_reference.into(),
                anchor: anchor_profile.contains(point),
                contact: separation_m <= BASIC_COLLISION_TOLERANCE_M,
            }
        })
        .collect();

    FixtureExchange {
        format: FIXTURE_EXCHANGE_FORMAT.into(),
        case_id: format!("{}-{}-{}", scenario.id(), conformity.id(), gravity.id()),
        coordinate: ExchangeCoordinate::default(),
        body_surface: ExchangeBodySurface {
            domain_id: BASIC_DOMAIN_ID,
            vertices,
            triangles: mesh.triangles,
        },
        cloth: ExchangeCloth { control_points },
        inputs: ExchangeInputs {
            world_gravity_m_per_s2: gravity.vector(),
            conformity: conformity.value(),
            collision_tolerance_m: BASIC_COLLISION_TOLERANCE_M,
        },
    }
}

fn vec3_array(value: DVec3) -> [f64; 3] {
    [value.x, value.y, value.z]
}

fn validate_exchange(exchange: &FixtureExchange) -> Result<(), FixtureExchangeError> {
    validation(
        exchange.format == FIXTURE_EXCHANGE_FORMAT,
        "unsupported format",
    )?;
    validation(!exchange.case_id.is_empty(), "case_id must not be empty")?;
    validation(
        exchange.coordinate == ExchangeCoordinate::default(),
        "unsupported coordinate declaration",
    )?;
    validation(
        !exchange.body_surface.vertices.is_empty(),
        "body surface must not be empty",
    )?;
    validation(
        !exchange.cloth.control_points.is_empty(),
        "cloth control points must not be empty",
    )?;

    for (index, vertex) in exchange.body_surface.vertices.iter().enumerate() {
        validation(
            finite3(vertex.position_m),
            &format!("body vertex {index} position is not finite"),
        )?;
        validation(
            finite3(vertex.normal) && DVec3::from_array(vertex.normal).length_squared() > 0.0,
            &format!("body vertex {index} normal is invalid"),
        )?;
        validation(
            finite2(vertex.uv) && normalized_uv(vertex.uv[0], vertex.uv[1]),
            &format!("body vertex {index} UV is invalid"),
        )?;
    }

    let vertex_count = exchange.body_surface.vertices.len();
    for (index, triangle) in exchange.body_surface.triangles.iter().enumerate() {
        validation(
            triangle.iter().all(|&vertex| vertex < vertex_count),
            &format!("triangle {index} vertex index is out of range"),
        )?;
    }

    let mut stable_ids = HashSet::new();
    let mut strip_orders = HashSet::new();
    for point in &exchange.cloth.control_points {
        validation(
            stable_ids.insert(point.stable_id),
            &format!("duplicate stable_id {}", point.stable_id),
        )?;
        validation(
            strip_orders.insert((point.strip_id, point.strip_order)),
            &format!(
                "duplicate strip/order {}/{}",
                point.strip_id, point.strip_order
            ),
        )?;
        validation(
            finite3(point.position_m),
            &format!("control point {} position is not finite", point.stable_id),
        )?;
        validation(
            point.surface_reference.domain_id == exchange.body_surface.domain_id,
            &format!(
                "control point {} references an unknown domain",
                point.stable_id
            ),
        )?;
        validation(
            point.surface_reference.u.is_finite()
                && point.surface_reference.v.is_finite()
                && normalized_uv(point.surface_reference.u, point.surface_reference.v),
            &format!(
                "control point {} SurfaceReference UV is invalid",
                point.stable_id
            ),
        )?;
    }

    validation(
        finite3(exchange.inputs.world_gravity_m_per_s2),
        "world gravity is not finite",
    )?;
    validation(
        exchange.inputs.conformity.is_finite()
            && (0.0..=1.0).contains(&exchange.inputs.conformity),
        "conformity must be finite and in [0,1]",
    )?;
    validation(
        exchange.inputs.collision_tolerance_m.is_finite()
            && exchange.inputs.collision_tolerance_m >= 0.0,
        "collision tolerance must be finite and non-negative",
    )?;

    Ok(())
}

fn validation(condition: bool, message: &str) -> Result<(), FixtureExchangeError> {
    if condition {
        Ok(())
    } else {
        Err(FixtureExchangeError::Validation(message.into()))
    }
}

fn finite3(value: [f64; 3]) -> bool {
    value.into_iter().all(f64::is_finite)
}

fn finite2(value: [f64; 2]) -> bool {
    value.into_iter().all(f64::is_finite)
}

fn normalized_uv(u: f64, v: f64) -> bool {
    (0.0..=1.0).contains(&u) && (0.0..=1.0).contains(&v)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{CLOTH_DEPTH_M, CLOTH_WIDTH_M};

    const EPS: f64 = 1.0e-12;

    fn sr001() -> FixtureExchange {
        generate_basic_exchange(
            BasicExchangeScenario::Sr001Flat,
            BasicGravityCase::Off,
            BasicConformityCase::C0,
        )
    }

    #[test]
    fn fxe_001_format_accepts_supported_and_rejects_unknown() {
        let exchange = sr001();
        assert!(exchange.validate().is_ok());

        let mut invalid = exchange;
        invalid.format = "sansacloth.validation.fixture-exchange/999".into();
        assert!(invalid.validate().is_err());
    }

    #[test]
    fn fxe_002_unit_preserves_sr001_dimensions() {
        let exchange = sr001();
        let xs: Vec<_> = exchange
            .body_surface
            .vertices
            .iter()
            .map(|vertex| vertex.position_m[0])
            .collect();
        let zs: Vec<_> = exchange
            .body_surface
            .vertices
            .iter()
            .map(|vertex| vertex.position_m[2])
            .collect();
        let width = xs.iter().copied().fold(f64::NEG_INFINITY, f64::max)
            - xs.iter().copied().fold(f64::INFINITY, f64::min);
        let depth = zs.iter().copied().fold(f64::NEG_INFINITY, f64::max)
            - zs.iter().copied().fold(f64::INFINITY, f64::min);
        assert!((width - CLOTH_WIDTH_M).abs() <= EPS);
        assert!((depth - CLOTH_DEPTH_M).abs() <= EPS);
    }

    #[test]
    fn fxe_003_body_mesh_has_basic_counts() {
        let exchange = sr001();
        assert_eq!(exchange.body_surface.vertices.len(), 147);
        assert_eq!(exchange.body_surface.triangles.len(), 240);
    }

    #[test]
    fn fxe_004_surface_reference_preserves_domain_and_uv() {
        let exchange = sr001();
        for point in &exchange.cloth.control_points {
            assert_eq!(point.surface_reference.domain_id, BASIC_DOMAIN_ID);
            assert!((0.0..=1.0).contains(&point.surface_reference.u));
            assert!((0.0..=1.0).contains(&point.surface_reference.v));
        }
    }

    #[test]
    fn fxe_005_control_point_identity_is_unique_and_contiguous() {
        let exchange = sr001();
        let ids: Vec<_> = exchange
            .cloth
            .control_points
            .iter()
            .map(|point| point.stable_id)
            .collect();
        assert_eq!(ids, (0..147_u64).collect::<Vec<_>>());
    }

    #[test]
    fn fxe_006_strip_topology_is_seven_by_twenty_one() {
        let exchange = sr001();
        for strip_id in 0..7_u64 {
            let points: Vec<_> = exchange
                .cloth
                .control_points
                .iter()
                .filter(|point| point.strip_id == strip_id)
                .collect();
            assert_eq!(points.len(), 21);
            for (order, point) in points.into_iter().enumerate() {
                assert_eq!(point.strip_order, order as u64);
            }
        }
    }

    #[test]
    fn fxe_007_sr001_anchor_and_contact_counts_are_preserved() {
        let exchange = sr001();
        assert_eq!(
            exchange
                .cloth
                .control_points
                .iter()
                .filter(|point| point.anchor)
                .count(),
            14
        );
        assert_eq!(
            exchange
                .cloth
                .control_points
                .iter()
                .filter(|point| point.contact)
                .count(),
            147
        );
    }

    #[test]
    fn fxe_008_sr003_bakes_side_geometry_and_one_edge_anchor() {
        let exchange = generate_basic_exchange(
            BasicExchangeScenario::Sr003ConvexSide,
            BasicGravityCase::Off,
            BasicConformityCase::C0,
        );
        let center = &exchange.body_surface.vertices[73];
        assert!((center.position_m[0] - 0.03).abs() <= EPS);
        assert!(center.position_m[1].abs() <= EPS);
        assert!(center.position_m[2].abs() <= EPS);
        assert!((center.normal[0] - 1.0).abs() <= EPS);
        assert!(center.normal[1].abs() <= EPS);
        assert_eq!(
            exchange
                .cloth
                .control_points
                .iter()
                .filter(|point| point.anchor)
                .count(),
            7
        );
    }

    #[test]
    fn fxe_009_case_inputs_round_trip() {
        let exchange = generate_basic_exchange(
            BasicExchangeScenario::Sr001Flat,
            BasicGravityCase::On,
            BasicConformityCase::C05,
        );
        assert_eq!(exchange.case_id, "SR-001-C05-G1");
        assert_eq!(
            exchange.inputs.world_gravity_m_per_s2,
            BASIC_GRAVITY_M_PER_S2
        );
        assert_eq!(exchange.inputs.conformity, 0.5);
        assert_eq!(
            exchange.inputs.collision_tolerance_m,
            BASIC_COLLISION_TOLERANCE_M
        );
    }

    #[test]
    fn fxe_010_json_round_trip_preserves_semantic_input() {
        let expected = sr001();
        let json = expected.to_json_pretty().unwrap();
        let actual = FixtureExchange::from_json(&json).unwrap();
        assert_exchange_semantically_eq(&actual, &expected);
    }

    fn assert_exchange_semantically_eq(actual: &FixtureExchange, expected: &FixtureExchange) {
        assert_eq!(actual.format, expected.format);
        assert_eq!(actual.case_id, expected.case_id);
        assert_eq!(actual.coordinate, expected.coordinate);
        assert_eq!(actual.body_surface.domain_id, expected.body_surface.domain_id);
        assert_eq!(actual.body_surface.triangles, expected.body_surface.triangles);
        assert_eq!(
            actual.body_surface.vertices.len(),
            expected.body_surface.vertices.len()
        );

        for (actual, expected) in actual
            .body_surface
            .vertices
            .iter()
            .zip(&expected.body_surface.vertices)
        {
            assert_float3_close(actual.position_m, expected.position_m);
            assert_float3_close(actual.normal, expected.normal);
            assert_float2_close(actual.uv, expected.uv);
        }

        assert_eq!(
            actual.cloth.control_points.len(),
            expected.cloth.control_points.len()
        );
        for (actual, expected) in actual
            .cloth
            .control_points
            .iter()
            .zip(&expected.cloth.control_points)
        {
            assert_eq!(actual.stable_id, expected.stable_id);
            assert_eq!(actual.strip_id, expected.strip_id);
            assert_eq!(actual.strip_order, expected.strip_order);
            assert_eq!(
                actual.surface_reference.domain_id,
                expected.surface_reference.domain_id
            );
            assert_float_close(actual.surface_reference.u, expected.surface_reference.u);
            assert_float_close(actual.surface_reference.v, expected.surface_reference.v);
            assert_float3_close(actual.position_m, expected.position_m);
            assert_eq!(actual.anchor, expected.anchor);
            assert_eq!(actual.contact, expected.contact);
        }

        assert_float3_close(
            actual.inputs.world_gravity_m_per_s2,
            expected.inputs.world_gravity_m_per_s2,
        );
        assert_float_close(actual.inputs.conformity, expected.inputs.conformity);
        assert_float_close(
            actual.inputs.collision_tolerance_m,
            expected.inputs.collision_tolerance_m,
        );
    }

    fn assert_float3_close(actual: [f64; 3], expected: [f64; 3]) {
        for (actual, expected) in actual.into_iter().zip(expected) {
            assert_float_close(actual, expected);
        }
    }

    fn assert_float2_close(actual: [f64; 2], expected: [f64; 2]) {
        for (actual, expected) in actual.into_iter().zip(expected) {
            assert_float_close(actual, expected);
        }
    }

    fn assert_float_close(actual: f64, expected: f64) {
        let tolerance = 2.0 * f64::EPSILON * expected.abs().max(1.0);
        assert!(
            (actual - expected).abs() <= tolerance,
            "float round-trip changed semantic value: actual={actual:?}, expected={expected:?}, tolerance={tolerance:?}"
        );
    }

    #[test]
    fn fxe_013_invalid_semantic_data_is_rejected() {
        let base = sr001();

        let mut invalid = base.clone();
        invalid.body_surface.vertices[0].uv = [-0.1, 0.0];
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.body_surface.vertices[0].normal = [0.0; 3];
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.body_surface.triangles[0][0] = invalid.body_surface.vertices.len();
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.cloth.control_points[1].stable_id = invalid.cloth.control_points[0].stable_id;
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.cloth.control_points[1].strip_id = invalid.cloth.control_points[0].strip_id;
        invalid.cloth.control_points[1].strip_order =
            invalid.cloth.control_points[0].strip_order;
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.cloth.control_points[0].surface_reference.domain_id = 999;
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.cloth.control_points[0].surface_reference.u = 1.1;
        assert!(invalid.validate().is_err());

        let mut invalid = base.clone();
        invalid.inputs.conformity = 1.1;
        assert!(invalid.validate().is_err());

        let mut invalid = base;
        invalid.inputs.collision_tolerance_m = -0.001;
        assert!(invalid.validate().is_err());
    }
}
