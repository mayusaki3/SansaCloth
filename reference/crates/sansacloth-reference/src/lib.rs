//! Reference Surface Solver for SansaCloth.
//!
//! This crate is a correctness and semantic reference implementation. It is
//! not an engine backend and does not define product performance targets.

use glam::DVec3;
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;

/// Local Body Surface information required by the Reference solver.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SurfaceQueryResult {
    pub surface_position_m: DVec3,
    pub surface_normal: DVec3,
    pub separation_m: f64,
}

/// Boundary used by the integrated solver to re-evaluate Body Surface data.
///
/// The query receives the current cloth position because separation must be
/// recomputed after stages that move the cloth. SurfaceReference preserves the
/// logical Body Surface location independently from runtime triangle identity.
pub trait SurfaceQuery {
    fn query(
        &self,
        current_position_m: DVec3,
        surface_reference: SurfaceReference,
    ) -> SurfaceQueryResult;
}

/// SR-001..005 adapter from analytic validation fixtures to SurfaceQuery.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AnalyticFixtureSurfaceQuery {
    pub fixture: AnalyticFixture,
}

impl SurfaceQuery for AnalyticFixtureSurfaceQuery {
    fn query(
        &self,
        current_position_m: DVec3,
        surface_reference: SurfaceReference,
    ) -> SurfaceQueryResult {
        assert!(current_position_m.is_finite());

        let surface_position_m = self
            .fixture
            .position(surface_reference.u, surface_reference.v);
        let surface_normal = self.fixture.normal(surface_reference.u);
        let separation_m = (current_position_m - surface_position_m).dot(surface_normal);

        assert!(surface_position_m.is_finite());
        assert!(surface_normal.is_finite());
        assert!(separation_m.is_finite());

        SurfaceQueryResult {
            surface_position_m,
            surface_normal,
            separation_m,
        }
    }
}

/// Input for the integrated ReferenceSurfaceSolver / 参照表面ソルバー.
///
/// Reference v1 integrates one already-ordered cloth strip. Topology discovery
/// and multi-strip decomposition remain outside this API.
#[derive(Clone, Debug, PartialEq)]
pub struct ReferenceSurfaceSolverInput {
    pub positions_m: Vec<DVec3>,
    pub surface_references: Vec<SurfaceReference>,
    pub is_anchor: Vec<bool>,
    pub is_contact: Vec<bool>,
    pub gravity_m_per_s2: DVec3,
    pub characteristic_length_m: f64,
    pub support_layout: GravitySupportLayout,
    pub conformity: f64,
    pub supported_strip: Vec<bool>,
}

/// Final implemented subset of SurfaceResponseResult / 表面応答結果.
///
/// Fields that Reference v1 does not yet compute are intentionally omitted
/// rather than populated with placeholder values.
#[derive(Clone, Debug, PartialEq)]
pub struct SurfaceResponseResult {
    pub positions_m: Vec<DVec3>,
    pub surface_references: Vec<SurfaceReference>,
    pub surface_positions_m: Vec<DVec3>,
    pub surface_normals: Vec<DVec3>,
    pub separation_m: Vec<f64>,
    pub support: Vec<SupportKind>,
}

/// Validation-only snapshots from each integrated solver stage.
#[derive(Clone, Debug, PartialEq)]
pub struct ReferenceSurfaceSolverDebug {
    pub initial_positions_m: Vec<DVec3>,
    pub support: Vec<SupportKind>,
    pub bridge_positions_m: Vec<DVec3>,
    pub gravity_positions_m: Vec<DVec3>,
    pub conformity_positions_m: Vec<DVec3>,
    pub collision_positions_m: Vec<DVec3>,
    pub final_result: SurfaceResponseResult,
}

/// Stateless integrated Reference Surface Solver.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ReferenceSurfaceSolver;

impl ReferenceSurfaceSolver {
    /// Solves one ordered strip and returns only the final SurfaceResponse.
    pub fn solve<Q: SurfaceQuery>(
        input: &ReferenceSurfaceSolverInput,
        surface_query: &Q,
        settings: ReferenceSolverSettings,
    ) -> SurfaceResponseResult {
        Self::solve_with_debug(input, surface_query, settings).final_result
    }

    /// Solves one ordered strip and retains validation snapshots for all stages.
    pub fn solve_with_debug<Q: SurfaceQuery>(
        input: &ReferenceSurfaceSolverInput,
        surface_query: &Q,
        settings: ReferenceSolverSettings,
    ) -> ReferenceSurfaceSolverDebug {
        let count = input.positions_m.len();
        assert_eq!(input.surface_references.len(), count);
        assert_eq!(input.is_anchor.len(), count);
        assert_eq!(input.is_contact.len(), count);
        assert_eq!(input.supported_strip.len(), count);

        let support_points: Vec<_> = (0..count)
            .map(|index| SupportPointInput {
                position_m: input.positions_m[index],
                is_anchor: input.is_anchor[index],
                is_contact: input.is_contact[index],
            })
            .collect();
        let support = resolve_support(&support_points);

        let bridge_positions_m = resolve_bridge(&BridgeStripInput {
            positions_m: input.positions_m.clone(),
            support: support.clone(),
        });

        let gravity_positions_m = apply_gravity_response(
            &GravityStripInput {
                positions_m: bridge_positions_m.clone(),
                support: support.clone(),
                gravity_m_per_s2: input.gravity_m_per_s2,
                characteristic_length_m: input.characteristic_length_m,
                support_layout: input.support_layout,
            },
            settings,
        );

        let after_gravity = query_surface_points(
            &gravity_positions_m,
            &input.surface_references,
            surface_query,
        );
        let conformity_positions_m = apply_conformity_response(
            &ConformityStripInput {
                positions_m: gravity_positions_m.clone(),
                desired_surface_positions_m: after_gravity
                    .iter()
                    .map(|query| query.surface_position_m)
                    .collect(),
                separation_m: after_gravity
                    .iter()
                    .map(|query| query.separation_m)
                    .collect(),
                support: support.clone(),
                conformity: input.conformity,
                supported_strip: input.supported_strip.clone(),
            },
            settings,
        );

        let after_conformity = query_surface_points(
            &conformity_positions_m,
            &input.surface_references,
            surface_query,
        );
        let collision_positions_m = apply_collision_response(
            &CollisionResponseInput {
                positions_m: conformity_positions_m.clone(),
                separation_m: after_conformity
                    .iter()
                    .map(|query| query.separation_m)
                    .collect(),
                surface_normals: after_conformity
                    .iter()
                    .map(|query| query.surface_normal)
                    .collect(),
            },
            settings,
        );

        let final_queries = query_surface_points(
            &collision_positions_m,
            &input.surface_references,
            surface_query,
        );
        let final_result = SurfaceResponseResult {
            positions_m: collision_positions_m.clone(),
            surface_references: input.surface_references.clone(),
            surface_positions_m: final_queries
                .iter()
                .map(|query| query.surface_position_m)
                .collect(),
            surface_normals: final_queries
                .iter()
                .map(|query| query.surface_normal)
                .collect(),
            separation_m: final_queries
                .iter()
                .map(|query| query.separation_m)
                .collect(),
            support: support.clone(),
        };

        ReferenceSurfaceSolverDebug {
            initial_positions_m: input.positions_m.clone(),
            support,
            bridge_positions_m,
            gravity_positions_m,
            conformity_positions_m,
            collision_positions_m,
            final_result,
        }
    }
}

fn query_surface_points<Q: SurfaceQuery>(
    positions_m: &[DVec3],
    surface_references: &[SurfaceReference],
    surface_query: &Q,
) -> Vec<SurfaceQueryResult> {
    assert_eq!(positions_m.len(), surface_references.len());
    positions_m
        .iter()
        .zip(surface_references)
        .map(|(&position_m, &surface_reference)| surface_query.query(position_m, surface_reference))
        .collect()
}

/// Minimal input state required by Support Resolution / 支持判定.
///
/// Contact is intentionally separate from Anchor. A contact point is not
/// automatically pinned by the Reference solver.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SupportPointInput {
    pub position_m: DVec3,
    pub is_anchor: bool,
    pub is_contact: bool,
}

/// Result of Support Resolution for one cloth control point.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SupportKind {
    /// Strong support created by an explicit Anchor.
    Anchor,
    /// No direct support has been established.
    Unsupported,
}

/// Resolves direct support without modifying point positions.
///
/// Reference v1 currently establishes only the confirmed rule that Anchor is
/// strong support. Contact alone is not promoted to a pin/support constraint.
/// Propagated SupportInfluence is a later solver concern and is not computed
/// by this stage.
pub fn resolve_support(points: &[SupportPointInput]) -> Vec<SupportKind> {
    points
        .iter()
        .map(|point| {
            if point.is_anchor {
                SupportKind::Anchor
            } else {
                SupportKind::Unsupported
            }
        })
        .collect()
}

/// Input for Bridge / 架橋 of one ordered cloth strip.
///
/// Reference v1 treats the strip parameter as an already-established cloth
/// ordering. It does not infer topology, body contact, gravity, or conformity.
#[derive(Clone, Debug, PartialEq)]
pub struct BridgeStripInput {
    pub positions_m: Vec<DVec3>,
    pub support: Vec<SupportKind>,
}

/// Builds the Reference v1 bridge baseline for an ordered cloth strip.
///
/// When both endpoints are supported, positions between them are replaced by
/// deterministic linear interpolation between the endpoint positions. With
/// only one supported endpoint, the initial cloth surface is preserved.
/// Supported endpoint positions are always preserved.
///
/// # Panics
/// Panics when positions and support arrays differ in length.
pub fn resolve_bridge(input: &BridgeStripInput) -> Vec<DVec3> {
    assert_eq!(input.positions_m.len(), input.support.len());

    let mut result = input.positions_m.clone();
    if result.len() < 2 {
        return result;
    }

    let first = 0;
    let last = result.len() - 1;
    let first_supported = input.support[first] != SupportKind::Unsupported;
    let last_supported = input.support[last] != SupportKind::Unsupported;

    if first_supported && last_supported {
        let p0 = input.positions_m[first];
        let p1 = input.positions_m[last];
        let denominator = last as f64;
        for (index, position) in result.iter_mut().enumerate().take(last).skip(1) {
            let t = index as f64 / denominator;
            *position = p0.lerp(p1, t);
        }
    }

    result
}

/// Reference-v1 settings for quasi-static gravity response.
///
/// `quasi_static_gravity_scale` is dimensionless and is deliberately not a
/// physical material property. It controls Reference-v1 displacement magnitude.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ReferenceSolverSettings {
    pub quasi_static_gravity_scale: f64,
    pub conformity_reach_m: f64,
    pub collision_tolerance_m: f64,
}

/// Support layout used by the fixture-specific Reference-v1 gravity weighting.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GravitySupportLayout {
    BothEdges,
    OneEdge,
}

/// Input for GravityResponse / 重力応答 of one ordered cloth strip.
#[derive(Clone, Debug, PartialEq)]
pub struct GravityStripInput {
    pub positions_m: Vec<DVec3>,
    pub support: Vec<SupportKind>,
    pub gravity_m_per_s2: DVec3,
    pub characteristic_length_m: f64,
    pub support_layout: GravitySupportLayout,
}

/// Applies the deterministic Reference-v1 quasi-static gravity approximation.
///
/// Gravity magnitude is not converted into displacement because Reference v1
/// has no time, mass, tension, or bending model. A non-zero gravity vector
/// supplies direction only. Displacement magnitude is
/// `characteristic_length_m * quasi_static_gravity_scale * support_weight`.
///
/// Both-edge fixture weight: `4*t*(1-t)`.
/// One-edge fixture weight: `t`.
///
/// Anchor positions are preserved exactly.
///
/// # Panics
/// Panics when positions and support arrays differ in length, or when settings
/// and characteristic length are non-finite/negative.
pub fn apply_gravity_response(
    input: &GravityStripInput,
    settings: ReferenceSolverSettings,
) -> Vec<DVec3> {
    assert_eq!(input.positions_m.len(), input.support.len());
    assert!(
        settings.quasi_static_gravity_scale.is_finite()
            && settings.quasi_static_gravity_scale >= 0.0
    );
    assert!(input.characteristic_length_m.is_finite() && input.characteristic_length_m >= 0.0);

    let Some(gravity_direction) = input.gravity_m_per_s2.try_normalize() else {
        return input.positions_m.clone();
    };

    let mut result = input.positions_m.clone();
    let last = result.len().saturating_sub(1);
    if last == 0 {
        return result;
    }

    for (index, position) in result.iter_mut().enumerate() {
        if input.support[index] != SupportKind::Unsupported {
            continue;
        }

        let t = index as f64 / last as f64;
        let support_weight = match input.support_layout {
            GravitySupportLayout::BothEdges => 4.0 * t * (1.0 - t),
            GravitySupportLayout::OneEdge => t,
        };
        let displacement_m =
            input.characteristic_length_m * settings.quasi_static_gravity_scale * support_weight;
        *position += gravity_direction * displacement_m;
    }

    result
}

/// Input for ConformityResponse / 表面追従応答 of one ordered cloth strip.
///
/// `desired_surface_positions_m` is supplied by the fixture/surface layer.
/// Reference v1 does not yet apply orientation weighting or surface filtering.
#[derive(Clone, Debug, PartialEq)]
pub struct ConformityStripInput {
    pub positions_m: Vec<DVec3>,
    pub desired_surface_positions_m: Vec<DVec3>,
    pub separation_m: Vec<f64>,
    pub support: Vec<SupportKind>,
    pub conformity: f64,
    /// True when the point belongs to a strip connected to established support.
    pub supported_strip: Vec<bool>,
}

/// Applies Reference-v1 Conformity / 表面追従性.
///
/// Effective conformity is
/// `conformity * distance_weight * support_eligibility`, where
/// `distance_weight = clamp(1 - separation / reach, 0, 1)`.
///
/// Anchors never move. Points outside reach, or points not connected to an
/// established supported strip, are not attracted toward the desired surface.
/// This stage does not perform gravity, collision correction, or slip.
///
/// # Panics
/// Panics for mismatched array lengths or invalid scalar settings.
pub fn apply_conformity_response(
    input: &ConformityStripInput,
    settings: ReferenceSolverSettings,
) -> Vec<DVec3> {
    let count = input.positions_m.len();
    assert_eq!(input.desired_surface_positions_m.len(), count);
    assert_eq!(input.separation_m.len(), count);
    assert_eq!(input.support.len(), count);
    assert_eq!(input.supported_strip.len(), count);
    assert!(input.conformity.is_finite() && (0.0..=1.0).contains(&input.conformity));
    assert!(settings.conformity_reach_m.is_finite() && settings.conformity_reach_m >= 0.0);

    if input.conformity == 0.0 || settings.conformity_reach_m == 0.0 {
        return input.positions_m.clone();
    }

    input
        .positions_m
        .iter()
        .enumerate()
        .map(|(index, &position)| {
            if input.support[index] != SupportKind::Unsupported || !input.supported_strip[index] {
                return position;
            }

            let separation = input.separation_m[index];
            if !separation.is_finite() || separation < 0.0 {
                return position;
            }

            let distance_weight = (1.0 - separation / settings.conformity_reach_m).clamp(0.0, 1.0);
            let effective = input.conformity * distance_weight;
            position.lerp(input.desired_surface_positions_m[index], effective)
        })
        .collect()
}

/// Input for CollisionResponse / 衝突応答.
///
/// `separation_m` is signed: negative values are penetration and positive
/// values are outside the body surface. `surface_normals` point outward.
#[derive(Clone, Debug, PartialEq)]
pub struct CollisionResponseInput {
    pub positions_m: Vec<DVec3>,
    pub separation_m: Vec<f64>,
    pub surface_normals: Vec<DVec3>,
}

/// Applies the Reference-v1 collision constraint.
///
/// A point whose signed separation is below `collision_tolerance_m` is moved
/// outward along the supplied surface normal by exactly the amount required to
/// reach the tolerance. Other points are unchanged. This stage does not model
/// cloth thickness, friction, gravity, conformity, slip, or softness.
///
/// # Panics
/// Panics for mismatched array lengths, invalid tolerance/separation values, or
/// a non-normalizable surface normal on a point that requires correction.
pub fn apply_collision_response(
    input: &CollisionResponseInput,
    settings: ReferenceSolverSettings,
) -> Vec<DVec3> {
    let count = input.positions_m.len();
    assert_eq!(input.separation_m.len(), count);
    assert_eq!(input.surface_normals.len(), count);
    assert!(settings.collision_tolerance_m.is_finite() && settings.collision_tolerance_m >= 0.0);

    input
        .positions_m
        .iter()
        .enumerate()
        .map(|(index, &position)| {
            let separation = input.separation_m[index];
            assert!(separation.is_finite());

            if separation >= settings.collision_tolerance_m {
                return position;
            }

            let normal = input.surface_normals[index]
                .try_normalize()
                .expect("collision correction requires a finite non-zero surface normal");
            let correction_m = settings.collision_tolerance_m - separation;
            position + normal * correction_m
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn surface_query_001_flat_surface_reports_signed_separation() {
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let reference = SurfaceReference::new(1, 0.5, 0.5).unwrap();

        let above = query.query(DVec3::new(0.0, 0.01, 0.0), reference);
        let below = query.query(DVec3::new(0.0, -0.01, 0.0), reference);

        assert_eq!(above.surface_position_m, DVec3::ZERO);
        assert_eq!(above.surface_normal, DVec3::Y);
        assert!((above.separation_m - 0.01).abs() <= f64::EPSILON);
        assert!((below.separation_m + 0.01).abs() <= f64::EPSILON);
    }

    #[test]
    fn surface_query_002_uses_surface_reference_for_analytic_fixture() {
        let fixture = AnalyticFixture::convex();
        let query = AnalyticFixtureSurfaceQuery { fixture };
        let reference = SurfaceReference::new(1, 0.5, 0.5).unwrap();
        let result = query.query(DVec3::new(0.0, 0.04, 0.0), reference);

        assert_eq!(result.surface_position_m, fixture.position(0.5, 0.5));
        assert_eq!(result.surface_normal, fixture.normal(0.5));
        assert!((result.separation_m - 0.01).abs() <= f64::EPSILON);
    }

    #[test]
    fn surface_query_003_recomputes_separation_after_position_change() {
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let reference = SurfaceReference::new(1, 0.5, 0.5).unwrap();

        let before = query.query(DVec3::new(0.0, 0.02, 0.0), reference);
        let after = query.query(DVec3::new(0.0, 0.005, 0.0), reference);

        assert!(after.separation_m < before.separation_m);
        assert!((after.separation_m - 0.005).abs() <= f64::EPSILON);
    }

    fn integrated_flat_input(gravity_m_per_s2: DVec3) -> ReferenceSurfaceSolverInput {
        let positions_m = vec![
            DVec3::new(-0.1, 0.0, 0.0),
            DVec3::new(-0.05, 0.0, 0.0),
            DVec3::new(0.0, 0.0, 0.0),
            DVec3::new(0.05, 0.0, 0.0),
            DVec3::new(0.1, 0.0, 0.0),
        ];
        ReferenceSurfaceSolverInput {
            positions_m,
            surface_references: (0..5)
                .map(|index| SurfaceReference::new(1, index as f64 / 4.0, 0.5).unwrap())
                .collect(),
            is_anchor: vec![true, false, false, false, true],
            is_contact: vec![true; 5],
            gravity_m_per_s2,
            characteristic_length_m: 0.10,
            support_layout: GravitySupportLayout::BothEdges,
            conformity: 0.0,
            supported_strip: vec![true; 5],
        }
    }

    #[test]
    fn int_solver_001_solve_matches_debug_final_result() {
        let input = integrated_flat_input(DVec3::ZERO);
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let direct = ReferenceSurfaceSolver::solve(&input, &query, gravity_settings());
        let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, gravity_settings());
        assert_eq!(direct, debug.final_result);
    }

    #[test]
    fn int_solver_002_flat_zero_gravity_preserves_positions() {
        let input = integrated_flat_input(DVec3::ZERO);
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let result = ReferenceSurfaceSolver::solve(&input, &query, gravity_settings());
        assert!(
            result
                .positions_m
                .iter()
                .zip(&input.positions_m)
                .all(|(actual, expected)| (*actual - *expected).length() <= f64::EPSILON)
        );
        assert!(
            result
                .separation_m
                .iter()
                .all(|value| value.abs() <= f64::EPSILON)
        );
    }

    #[test]
    fn int_solver_003_requeries_surface_before_collision() {
        let input = integrated_flat_input(DVec3::NEG_Y);
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, gravity_settings());

        assert!(debug.gravity_positions_m[2].y < 0.0);
        assert_eq!(debug.collision_positions_m[2].y, 0.0);
        assert!(debug.final_result.separation_m[2].abs() <= f64::EPSILON);
    }

    #[test]
    fn int_solver_004_preserves_anchors_through_all_stages() {
        let input = integrated_flat_input(DVec3::NEG_Y);
        let query = AnalyticFixtureSurfaceQuery {
            fixture: AnalyticFixture::flat(),
        };
        let debug = ReferenceSurfaceSolver::solve_with_debug(&input, &query, gravity_settings());

        for &index in &[0, 4] {
            assert_eq!(debug.bridge_positions_m[index], input.positions_m[index]);
            assert_eq!(debug.gravity_positions_m[index], input.positions_m[index]);
            assert_eq!(
                debug.conformity_positions_m[index],
                input.positions_m[index]
            );
            assert_eq!(debug.collision_positions_m[index], input.positions_m[index]);
        }
    }

    fn point(is_anchor: bool, is_contact: bool) -> SupportPointInput {
        SupportPointInput {
            position_m: DVec3::ZERO,
            is_anchor,
            is_contact,
        }
    }

    #[test]
    fn ref_sup_001_anchor_becomes_support() {
        let result = resolve_support(&[point(true, false)]);
        assert_eq!(result, vec![SupportKind::Anchor]);
    }

    #[test]
    fn ref_sup_002_contact_does_not_pin_all_points() {
        let result = resolve_support(&[point(false, true)]);
        assert_eq!(result, vec![SupportKind::Unsupported]);
    }

    #[test]
    fn ref_sup_003_separated_unanchored_point_is_unsupported() {
        let result = resolve_support(&[point(false, false)]);
        assert_eq!(result, vec![SupportKind::Unsupported]);
    }

    #[test]
    fn ref_bri_001_preserves_supported_endpoint_positions() {
        let input = BridgeStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.2, 0.0),
                DVec3::new(0.0, -2.0, 0.0),
                DVec3::new(1.0, 0.4, 0.0),
            ],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
        };
        let result = resolve_bridge(&input);
        assert_eq!(result[0], input.positions_m[0]);
        assert_eq!(result[2], input.positions_m[2]);
    }

    #[test]
    fn ref_bri_002_builds_continuous_linear_bridge_between_supported_edges() {
        let input = BridgeStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(-0.5, -3.0, 0.0),
                DVec3::new(0.0, -4.0, 0.0),
                DVec3::new(0.5, -3.0, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
        };
        let result = resolve_bridge(&input);
        assert_eq!(result[1], DVec3::new(-0.5, 0.0, 0.0));
        assert_eq!(result[2], DVec3::new(0.0, 0.0, 0.0));
        assert_eq!(result[3], DVec3::new(0.5, 0.0, 0.0));
    }

    #[test]
    fn ref_bri_003_bridge_does_not_follow_concave_body_surface() {
        let input = BridgeStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(0.0, 0.0, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
        };
        let result = resolve_bridge(&input);

        // A hypothetical concave body bottom at y=-0.5 is intentionally not
        // an input to Bridge; the baseline remains between its supports.
        assert_eq!(result[1], DVec3::ZERO);
        assert_ne!(result[1].y, -0.5);
    }

    #[test]
    fn one_edge_bridge_preserves_initial_cloth_surface() {
        let input = BridgeStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(0.0, 0.25, 0.0),
                DVec3::new(1.0, 0.5, 0.0),
            ],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
            ],
        };
        assert_eq!(resolve_bridge(&input), input.positions_m);
    }

    fn gravity_input(gravity_m_per_s2: DVec3, layout: GravitySupportLayout) -> GravityStripInput {
        GravityStripInput {
            positions_m: vec![DVec3::ZERO; 5],
            support: match layout {
                GravitySupportLayout::BothEdges => vec![
                    SupportKind::Anchor,
                    SupportKind::Unsupported,
                    SupportKind::Unsupported,
                    SupportKind::Unsupported,
                    SupportKind::Anchor,
                ],
                GravitySupportLayout::OneEdge => vec![
                    SupportKind::Anchor,
                    SupportKind::Unsupported,
                    SupportKind::Unsupported,
                    SupportKind::Unsupported,
                    SupportKind::Unsupported,
                ],
            },
            gravity_m_per_s2,
            characteristic_length_m: 0.10,
            support_layout: layout,
        }
    }

    fn gravity_settings() -> ReferenceSolverSettings {
        ReferenceSolverSettings {
            quasi_static_gravity_scale: 0.1,
            conformity_reach_m: 0.02,
            collision_tolerance_m: 0.0,
        }
    }

    #[test]
    fn grv_001_zero_gravity_has_zero_displacement() {
        let input = gravity_input(DVec3::ZERO, GravitySupportLayout::BothEdges);
        assert_eq!(
            apply_gravity_response(&input, gravity_settings()),
            input.positions_m
        );
    }

    #[test]
    fn grv_002_anchor_displacement_is_zero() {
        let input = gravity_input(
            DVec3::new(0.0, -9.80665, 0.0),
            GravitySupportLayout::BothEdges,
        );
        let result = apply_gravity_response(&input, gravity_settings());
        assert_eq!(result[0], input.positions_m[0]);
        assert_eq!(result[4], input.positions_m[4]);
    }

    #[test]
    fn grv_003_both_edge_center_has_largest_tendency() {
        let input = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        let result = apply_gravity_response(&input, gravity_settings());
        assert!(result[2].y < result[1].y);
        assert!(result[2].y < result[3].y);
    }

    #[test]
    fn grv_004_one_edge_response_increases_away_from_anchor() {
        let input = gravity_input(DVec3::NEG_Y, GravitySupportLayout::OneEdge);
        let result = apply_gravity_response(&input, gravity_settings());
        assert!(result[1].y > result[2].y);
        assert!(result[2].y > result[3].y);
        assert!(result[3].y > result[4].y);
    }

    #[test]
    fn grv_005_reversing_gravity_reverses_displacement() {
        let down = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        let up = gravity_input(DVec3::Y, GravitySupportLayout::BothEdges);
        let down_result = apply_gravity_response(&down, gravity_settings());
        let up_result = apply_gravity_response(&up, gravity_settings());
        assert_eq!(down_result[2], -up_result[2]);
    }

    #[test]
    fn grv_006_body_only_rotation_does_not_change_world_gravity_input() {
        let input = gravity_input(
            DVec3::new(0.0, -9.80665, 0.0),
            GravitySupportLayout::BothEdges,
        );
        let before = input.gravity_m_per_s2;
        let _ = apply_gravity_response(&input, gravity_settings());
        assert_eq!(input.gravity_m_per_s2, before);
    }

    #[test]
    fn grv_007_rotated_gravity_follows_supplied_direction() {
        let input = gravity_input(DVec3::X, GravitySupportLayout::BothEdges);
        let result = apply_gravity_response(&input, gravity_settings());
        assert!(result[2].x > 0.0);
        assert_eq!(result[2].y, 0.0);
        assert_eq!(result[2].z, 0.0);
    }

    #[test]
    fn grv_008_gravity_stage_does_not_perform_collision_correction() {
        let mut input = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        input.positions_m[2] = DVec3::new(0.0, -1.0, 0.0);
        let result = apply_gravity_response(&input, gravity_settings());
        assert!(result[2].y < -1.0);
    }

    #[test]
    fn grv_009_gravity_stage_has_no_conformity_input_or_response() {
        let input = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        let result = apply_gravity_response(&input, gravity_settings());
        assert_eq!(result[2].x, input.positions_m[2].x);
        assert_eq!(result[2].z, input.positions_m[2].z);
    }

    #[test]
    fn grv_010_is_deterministic() {
        let input = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        assert_eq!(
            apply_gravity_response(&input, gravity_settings()),
            apply_gravity_response(&input, gravity_settings())
        );
    }

    #[test]
    fn grv_011_resolution_does_not_change_center_displacement() {
        let low = gravity_input(DVec3::NEG_Y, GravitySupportLayout::BothEdges);
        let high = GravityStripInput {
            positions_m: vec![DVec3::ZERO; 9],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
            gravity_m_per_s2: DVec3::NEG_Y,
            characteristic_length_m: 0.10,
            support_layout: GravitySupportLayout::BothEdges,
        };
        let low_result = apply_gravity_response(&low, gravity_settings());
        let high_result = apply_gravity_response(&high, gravity_settings());
        assert_eq!(low_result[2], high_result[4]);
    }

    #[test]
    fn grv_012_outputs_are_finite() {
        let input = gravity_input(
            DVec3::new(1.0, -9.80665, 2.0),
            GravitySupportLayout::BothEdges,
        );
        let result = apply_gravity_response(&input, gravity_settings());
        assert!(result.iter().all(|position| position.is_finite()));
    }

    fn conformity_input(conformity: f64) -> ConformityStripInput {
        ConformityStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(0.0, 0.01, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            desired_surface_positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(0.0, 0.0, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            separation_m: vec![0.0, 0.01, 0.0],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
            conformity,
            supported_strip: vec![true, true, true],
        }
    }

    #[test]
    fn conf_001_zero_conformity_has_zero_stage_displacement() {
        let input = conformity_input(0.0);
        assert_eq!(
            apply_conformity_response(&input, gravity_settings()),
            input.positions_m
        );
    }

    #[test]
    fn conf_002_anchor_displacement_is_zero() {
        let input = conformity_input(1.0);
        let result = apply_conformity_response(&input, gravity_settings());
        assert_eq!(result[0], input.positions_m[0]);
        assert_eq!(result[2], input.positions_m[2]);
    }

    #[test]
    fn conf_003_outside_reach_unsupported_point_is_not_attracted() {
        let mut input = conformity_input(1.0);
        input.separation_m[1] = 0.03;
        let result = apply_conformity_response(&input, gravity_settings());
        assert_eq!(result[1], input.positions_m[1]);
    }

    #[test]
    fn conf_004_eligible_response_increases_with_conformity() {
        let low = conformity_input(0.5);
        let high = conformity_input(1.0);
        let low_result = apply_conformity_response(&low, gravity_settings());
        let high_result = apply_conformity_response(&high, gravity_settings());
        let low_delta = (low_result[1] - low.positions_m[1]).length();
        let high_delta = (high_result[1] - high.positions_m[1]).length();
        assert!(high_delta > low_delta);
    }

    #[test]
    fn conf_005_full_conformity_outside_reach_has_no_attraction() {
        let mut input = conformity_input(1.0);
        input.separation_m[1] = gravity_settings().conformity_reach_m;
        let result = apply_conformity_response(&input, gravity_settings());
        assert_eq!(result[1], input.positions_m[1]);
    }

    #[test]
    fn conf_006_conformity_does_not_replace_collision_response() {
        let mut input = conformity_input(1.0);
        input.positions_m[1] = DVec3::new(0.0, -0.01, 0.0);
        input.desired_surface_positions_m[1] = DVec3::ZERO;
        input.separation_m[1] = 0.01;
        let result = apply_conformity_response(&input, gravity_settings());
        assert!(result[1].y < 0.0);
    }

    #[test]
    fn conf_007_shallow_concavity_can_receive_response() {
        let mut input = conformity_input(1.0);
        input.desired_surface_positions_m[1] = DVec3::new(0.0, -0.01, 0.0);
        let result = apply_conformity_response(&input, gravity_settings());
        assert!(result[1].y < input.positions_m[1].y);
    }

    #[test]
    fn conf_008_deep_concavity_does_not_force_bottom_contact() {
        let mut input = conformity_input(1.0);
        input.desired_surface_positions_m[1] = DVec3::new(0.0, -0.05, 0.0);
        input.separation_m[1] = 0.05;
        let result = apply_conformity_response(&input, gravity_settings());
        assert_eq!(result[1], input.positions_m[1]);
    }

    #[test]
    fn conf_009_completely_unsupported_region_has_no_adhesion() {
        let mut input = conformity_input(1.0);
        input.support = vec![SupportKind::Unsupported; 3];
        input.supported_strip = vec![false; 3];
        let result = apply_conformity_response(&input, gravity_settings());
        assert_eq!(result, input.positions_m);
    }

    #[test]
    fn conf_010_coordinate_rotation_equivalence() {
        let input = conformity_input(1.0);
        let result = apply_conformity_response(&input, gravity_settings());

        let mut rotated = input.clone();
        rotated.positions_m = input
            .positions_m
            .iter()
            .map(|p| DVec3::new(p.y, -p.x, p.z))
            .collect();
        rotated.desired_surface_positions_m = input
            .desired_surface_positions_m
            .iter()
            .map(|p| DVec3::new(p.y, -p.x, p.z))
            .collect();
        let rotated_result = apply_conformity_response(&rotated, gravity_settings());
        let expected: Vec<_> = result.iter().map(|p| DVec3::new(p.y, -p.x, p.z)).collect();
        assert_eq!(rotated_result, expected);
    }

    #[test]
    fn conf_011_does_not_mutate_external_gravity_state() {
        let gravity = DVec3::new(0.0, -9.80665, 0.0);
        let input = conformity_input(1.0);
        let _ = apply_conformity_response(&input, gravity_settings());
        assert_eq!(gravity, DVec3::new(0.0, -9.80665, 0.0));
    }

    #[test]
    fn conf_012_does_not_mutate_anchor_state() {
        let input = conformity_input(1.0);
        let before = input.support.clone();
        let _ = apply_conformity_response(&input, gravity_settings());
        assert_eq!(input.support, before);
    }

    #[test]
    fn conf_013_is_deterministic() {
        let input = conformity_input(0.5);
        assert_eq!(
            apply_conformity_response(&input, gravity_settings()),
            apply_conformity_response(&input, gravity_settings())
        );
    }

    #[test]
    fn conf_014_outputs_are_finite() {
        let input = conformity_input(1.0);
        let result = apply_conformity_response(&input, gravity_settings());
        assert!(result.iter().all(|position| position.is_finite()));
    }

    #[test]
    fn conf_015_resolution_preserves_center_response() {
        let low = conformity_input(1.0);
        let high = ConformityStripInput {
            positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(-0.5, 0.01, 0.0),
                DVec3::new(0.0, 0.01, 0.0),
                DVec3::new(0.5, 0.01, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            desired_surface_positions_m: vec![
                DVec3::new(-1.0, 0.0, 0.0),
                DVec3::new(-0.5, 0.0, 0.0),
                DVec3::new(0.0, 0.0, 0.0),
                DVec3::new(0.5, 0.0, 0.0),
                DVec3::new(1.0, 0.0, 0.0),
            ],
            separation_m: vec![0.0, 0.01, 0.01, 0.01, 0.0],
            support: vec![
                SupportKind::Anchor,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Unsupported,
                SupportKind::Anchor,
            ],
            conformity: 1.0,
            supported_strip: vec![true; 5],
        };
        let low_result = apply_conformity_response(&low, gravity_settings());
        let high_result = apply_conformity_response(&high, gravity_settings());
        assert_eq!(low_result[1], high_result[2]);
    }

    fn collision_input(separation_m: f64) -> CollisionResponseInput {
        CollisionResponseInput {
            positions_m: vec![DVec3::new(0.0, separation_m, 0.0)],
            separation_m: vec![separation_m],
            surface_normals: vec![DVec3::Y],
        }
    }

    #[test]
    fn ref_col_001_non_penetrating_point_is_not_unnecessarily_moved() {
        let input = collision_input(0.01);
        assert_eq!(
            apply_collision_response(&input, gravity_settings()),
            input.positions_m
        );
    }

    #[test]
    fn ref_col_002_penetrating_point_is_corrected_outward() {
        let input = collision_input(-0.01);
        let result = apply_collision_response(&input, gravity_settings());
        assert_eq!(result[0], DVec3::ZERO);
        assert!(result[0].y > input.positions_m[0].y);
    }

    #[test]
    fn ref_col_003_final_penetration_is_within_configured_tolerance() {
        let mut settings = gravity_settings();
        settings.collision_tolerance_m = 0.002;
        let input = collision_input(-0.01);
        let result = apply_collision_response(&input, settings);
        let correction = result[0] - input.positions_m[0];
        let final_separation = input.separation_m[0] + correction.dot(DVec3::Y);
        assert!(final_separation >= settings.collision_tolerance_m);
        assert!((final_separation - settings.collision_tolerance_m).abs() <= f64::EPSILON);
    }

    #[test]
    fn support_resolution_preserves_input_positions() {
        let points = vec![
            SupportPointInput {
                position_m: DVec3::new(1.0, 2.0, 3.0),
                is_anchor: true,
                is_contact: true,
            },
            SupportPointInput {
                position_m: DVec3::new(-1.0, 0.5, 4.0),
                is_anchor: false,
                is_contact: true,
            },
        ];
        let before = points.clone();
        let _ = resolve_support(&points);
        assert_eq!(points, before);
    }
}
