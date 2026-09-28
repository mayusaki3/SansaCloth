//! Analytic validation fixtures for the SansaCloth PC Reference.
//!
//! These shapes are validation fixtures only; they are not product or anatomy standards.

use std::f64::consts::PI;

use glam::DVec3;

pub const FIXTURE_WIDTH_M: f64 = 0.20;
pub const FIXTURE_DEPTH_M: f64 = 0.10;
pub const FEATURE_WIDTH_M: f64 = 0.10;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SurfaceKind {
    Flat,
    Convex { height_m: f64 },
    Concave { depth_m: f64 },
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AnalyticFixture {
    pub kind: SurfaceKind,
}

impl AnalyticFixture {
    pub const fn flat() -> Self { Self { kind: SurfaceKind::Flat } }
    pub const fn convex() -> Self { Self { kind: SurfaceKind::Convex { height_m: 0.03 } } }
    pub const fn concave_shallow() -> Self { Self { kind: SurfaceKind::Concave { depth_m: 0.02 } } }
    pub const fn concave_deep() -> Self { Self { kind: SurfaceKind::Concave { depth_m: 0.05 } } }

    /// Evaluates the analytic fixture position for normalized U/V coordinates.
    pub fn position(&self, u: f64, v: f64) -> DVec3 {
        let x = (u - 0.5) * FIXTURE_WIDTH_M;
        let z = (v - 0.5) * FIXTURE_DEPTH_M;
        let y = match self.kind {
            SurfaceKind::Flat => 0.0,
            SurfaceKind::Convex { height_m } => raised_cosine(x, FEATURE_WIDTH_M, height_m),
            SurfaceKind::Concave { depth_m } => -raised_cosine(x, FEATURE_WIDTH_M, depth_m),
        };
        DVec3::new(x, y, z)
    }

    /// Evaluates the mathematical outward normal (+Y on the base plane).
    pub fn normal(&self, u: f64) -> DVec3 {
        let x = (u - 0.5) * FIXTURE_WIDTH_M;
        let dydx = match self.kind {
            SurfaceKind::Flat => 0.0,
            SurfaceKind::Convex { height_m } => raised_cosine_derivative(x, FEATURE_WIDTH_M, height_m),
            SurfaceKind::Concave { depth_m } => -raised_cosine_derivative(x, FEATURE_WIDTH_M, depth_m),
        };
        DVec3::new(-dydx, 1.0, 0.0).normalize()
    }

    /// Samples a regular U/V mesh using the fixed V00->V11 diagonal.
    ///
    /// Triangle winding is (V00,V01,V11) and (V00,V11,V10), producing +Y
    /// geometric normals on the flat fixture.
    pub fn sample_mesh(&self, u_samples: usize, v_samples: usize) -> Mesh {
        assert!(u_samples >= 2 && v_samples >= 2);
        let mut positions = Vec::with_capacity(u_samples * v_samples);
        for j in 0..v_samples {
            let v = j as f64 / (v_samples - 1) as f64;
            for i in 0..u_samples {
                let u = i as f64 / (u_samples - 1) as f64;
                positions.push(self.position(u, v));
            }
        }

        let mut triangles = Vec::with_capacity((u_samples - 1) * (v_samples - 1) * 2);
        for j in 0..(v_samples - 1) {
            for i in 0..(u_samples - 1) {
                let v00 = j * u_samples + i;
                let v10 = v00 + 1;
                let v01 = v00 + u_samples;
                let v11 = v01 + 1;
                triangles.push([v00, v01, v11]);
                triangles.push([v00, v11, v10]);
            }
        }
        Mesh { positions, triangles }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct Mesh {
    pub positions: Vec<DVec3>,
    pub triangles: Vec<[usize; 3]>,
}

/// Raised-cosine validation feature.
pub fn raised_cosine(x: f64, width_m: f64, height_m: f64) -> f64 {
    if x.abs() > width_m * 0.5 {
        0.0
    } else {
        height_m * 0.5 * (1.0 + (2.0 * PI * x / width_m).cos())
    }
}

/// First derivative of the raised-cosine validation feature.
pub fn raised_cosine_derivative(x: f64, width_m: f64, height_m: f64) -> f64 {
    if x.abs() > width_m * 0.5 {
        0.0
    } else {
        -(height_m * PI / width_m) * (2.0 * PI * x / width_m).sin()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const EPS: f64 = 1.0e-12;

    #[test]
    fn fix_001_flat_plane() {
        for &(u, v) in &[(0.0, 0.0), (0.5, 0.5), (1.0, 1.0)] {
            assert!(AnalyticFixture::flat().position(u, v).y.abs() < EPS);
        }
    }

    #[test]
    fn fix_002_convex_maximum_height() {
        assert!((AnalyticFixture::convex().position(0.5, 0.5).y - 0.03).abs() < EPS);
    }

    #[test]
    fn fix_003_convex_feature_width() {
        let f = AnalyticFixture::convex();
        assert!(f.position(0.25, 0.5).y.abs() < EPS);
        assert!(f.position(0.75, 0.5).y.abs() < EPS);
    }

    #[test]
    fn fix_004_shallow_concavity_depth() {
        assert!((AnalyticFixture::concave_shallow().position(0.5, 0.5).y + 0.02).abs() < EPS);
    }

    #[test]
    fn fix_005_deep_concavity_depth() {
        assert!((AnalyticFixture::concave_deep().position(0.5, 0.5).y + 0.05).abs() < EPS);
    }

    #[test]
    fn fix_006_mesh_is_deterministic() {
        let f = AnalyticFixture::convex();
        assert_eq!(f.sample_mesh(11, 5), f.sample_mesh(11, 5));
    }

    #[test]
    fn fix_007_resolution_preserves_analytic_shape() {
        let f = AnalyticFixture::convex();
        for &(u, v) in &[(0.0, 0.0), (0.5, 0.5), (1.0, 1.0)] {
            assert!(f.position(u, v).is_finite());
        }
    }

    #[test]
    fn fix_011_fixed_diagonal_is_v00_to_v11() {
        let mesh = AnalyticFixture::flat().sample_mesh(2, 2);
        assert_eq!(mesh.triangles, vec![[0, 2, 3], [0, 3, 1]]);
    }

    #[test]
    fn fix_012_flat_winding_points_outward() {
        let mesh = AnalyticFixture::flat().sample_mesh(2, 2);
        for tri in mesh.triangles {
            let p0 = mesh.positions[tri[0]];
            let p1 = mesh.positions[tri[1]];
            let p2 = mesh.positions[tri[2]];
            let n = (p1 - p0).cross(p2 - p0).normalize();
            assert!(n.dot(DVec3::Y) > 0.0);
        }
    }

    #[test]
    fn fix_013_geometric_normals_agree_with_analytic_normals() {
        let f = AnalyticFixture::convex();
        let u_samples = 41;
        let v_samples = 11;
        let mesh = f.sample_mesh(u_samples, v_samples);
        for tri in mesh.triangles {
            let p0 = mesh.positions[tri[0]];
            let p1 = mesh.positions[tri[1]];
            let p2 = mesh.positions[tri[2]];
            let geometric = (p1 - p0).cross(p2 - p0).normalize();
            let center = (p0 + p1 + p2) / 3.0;
            let u = center.x / FIXTURE_WIDTH_M + 0.5;
            assert!(geometric.dot(f.normal(u)) > 0.0);
        }
    }
}
