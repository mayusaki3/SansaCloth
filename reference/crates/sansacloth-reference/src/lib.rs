//! Reference Surface Solver for SansaCloth.
//!
//! This crate is a correctness and semantic reference implementation. It is
//! not an engine backend and does not define product performance targets.

use glam::DVec3;

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

#[cfg(test)]
mod tests {
    use super::*;

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
