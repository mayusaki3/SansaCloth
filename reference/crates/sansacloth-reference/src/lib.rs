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
