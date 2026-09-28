//! Engine-independent data types used by the SansaCloth PC Reference.
//!
//! Length values stored by this crate use metres as the canonical unit.

use glam::{DMat3, DVec3};

/// A stable authoring-space reference to a logical surface domain.
///
/// The concrete persistent representation of `domain_id` is provisional for
/// the Reference implementation and is not the final product serialization.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SurfaceReference {
    pub domain_id: u64,
    pub u: f64,
    pub v: f64,
}

impl SurfaceReference {
    /// Creates a surface reference when the normalized coordinates are valid.
    ///
    /// # Arguments
    /// * `domain_id` - Reference-local stable surface domain identifier.
    /// * `u`, `v` - Normalized surface coordinates in the inclusive range 0..=1.
    ///
    /// # Returns
    /// `Some` for finite in-range coordinates, otherwise `None`.
    pub fn new(domain_id: u64, u: f64, v: f64) -> Option<Self> {
        (u.is_finite() && v.is_finite() && (0.0..=1.0).contains(&u) && (0.0..=1.0).contains(&v))
            .then_some(Self { domain_id, u, v })
    }
}

/// Barycentric coordinates for a triangle binding.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Barycentric {
    pub a: f64,
    pub b: f64,
    pub c: f64,
}

impl Barycentric {
    /// Normalizes finite barycentric weights whose sum is non-zero.
    pub fn normalized(self) -> Option<Self> {
        let sum = self.a + self.b + self.c;
        if !self.a.is_finite()
            || !self.b.is_finite()
            || !self.c.is_finite()
            || !sum.is_finite()
            || sum.abs() <= f64::EPSILON
        {
            return None;
        }
        Some(Self {
            a: self.a / sum,
            b: self.b / sum,
            c: self.c / sum,
        })
    }
}

/// Local orthonormal frame attached to a surface position.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SurfaceFrame {
    pub origin_m: DVec3,
    pub normal: DVec3,
    pub tangent_u: DVec3,
    pub tangent_v: DVec3,
}

impl SurfaceFrame {
    /// Builds an orthonormal frame from an origin, normal, and U tangent hint.
    ///
    /// Returns `None` for non-finite, zero-length, or parallel input vectors.
    pub fn from_normal_and_tangent_u(
        origin_m: DVec3,
        normal: DVec3,
        tangent_u_hint: DVec3,
    ) -> Option<Self> {
        if !origin_m.is_finite() || !normal.is_finite() || !tangent_u_hint.is_finite() {
            return None;
        }
        let normal = normal.try_normalize()?;
        let tangent_u = (tangent_u_hint - normal * tangent_u_hint.dot(normal)).try_normalize()?;
        let tangent_v = normal.cross(tangent_u).try_normalize()?;
        Some(Self {
            origin_m,
            normal,
            tangent_u,
            tangent_v,
        })
    }

    /// Reconstructs a world/local position from a surface-relative offset in metres.
    pub fn position_from_offset(&self, offset: SurfaceOffset) -> DVec3 {
        self.origin_m
            + self.tangent_u * offset.tangent_u_m
            + self.tangent_v * offset.tangent_v_m
            + self.normal * offset.normal_m
    }

    /// Returns the frame basis as a matrix whose columns are U, V, and normal.
    pub fn basis(&self) -> DMat3 {
        DMat3::from_cols(self.tangent_u, self.tangent_v, self.normal)
    }
}

/// Surface-relative displacement. All components use metres.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SurfaceOffset {
    pub tangent_u_m: f64,
    pub tangent_v_m: f64,
    pub normal_m: f64,
}

/// Converts centimetres used by authoring UI into canonical metres.
pub fn centimetres_to_metres(value_cm: f64) -> f64 {
    value_cm * 0.01
}

/// Converts millimetres used by authoring UI into canonical metres.
pub fn millimetres_to_metres(value_mm: f64) -> f64 {
    value_mm * 0.001
}

#[cfg(test)]
mod tests {
    use super::*;

    const EPS: f64 = 1.0e-12;

    #[test]
    fn core_001_internal_length_is_metres() {
        assert!((centimetres_to_metres(100.0) - 1.0).abs() < EPS);
    }

    #[test]
    fn core_002_centimetres_convert_to_metres() {
        assert!((centimetres_to_metres(2.5) - 0.025).abs() < EPS);
    }

    #[test]
    fn core_003_millimetres_convert_to_metres() {
        assert!((millimetres_to_metres(25.0) - 0.025).abs() < EPS);
    }

    #[test]
    fn core_004_surface_reference_rejects_invalid_coordinates() {
        assert!(SurfaceReference::new(1, 0.5, 0.5).is_some());
        assert!(SurfaceReference::new(1, -0.1, 0.5).is_none());
        assert!(SurfaceReference::new(1, 0.5, 1.1).is_none());
        assert!(SurfaceReference::new(1, f64::NAN, 0.5).is_none());
    }

    #[test]
    fn core_005_barycentric_normalization() {
        let b = Barycentric {
            a: 2.0,
            b: 3.0,
            c: 5.0,
        }
        .normalized()
        .unwrap();
        assert!((b.a + b.b + b.c - 1.0).abs() < EPS);
        assert!(
            Barycentric {
                a: 0.0,
                b: 0.0,
                c: 0.0
            }
            .normalized()
            .is_none()
        );
    }

    #[test]
    fn core_006_surface_frame_is_orthonormal() {
        let frame =
            SurfaceFrame::from_normal_and_tangent_u(DVec3::ZERO, DVec3::Y, DVec3::X).unwrap();
        assert!(frame.normal.dot(frame.tangent_u).abs() < EPS);
        assert!(frame.normal.dot(frame.tangent_v).abs() < EPS);
        assert!(frame.tangent_u.dot(frame.tangent_v).abs() < EPS);
        assert!((frame.normal.length() - 1.0).abs() < EPS);
        assert!((frame.tangent_u.length() - 1.0).abs() < EPS);
        assert!((frame.tangent_v.length() - 1.0).abs() < EPS);
    }

    #[test]
    fn core_007_surface_offset_reconstructs_position() {
        let frame =
            SurfaceFrame::from_normal_and_tangent_u(DVec3::new(1.0, 2.0, 3.0), DVec3::Y, DVec3::X)
                .unwrap();
        let p = frame.position_from_offset(SurfaceOffset {
            tangent_u_m: 0.1,
            tangent_v_m: 0.2,
            normal_m: 0.3,
        });
        assert!((p - DVec3::new(1.1, 2.3, 2.8)).length() < EPS);
    }
}
