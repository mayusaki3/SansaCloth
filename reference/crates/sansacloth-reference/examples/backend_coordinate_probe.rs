use glam::{DQuat, DVec3};
use sansacloth_core::SurfaceReference;
use sansacloth_fixture::AnalyticFixture;
use sansacloth_reference::{
    FixtureTransform, SurfaceQuery, TransformedAnalyticFixtureSurfaceQuery,
};

fn print_scalar(key: &str, value: f64) {
    println!("SANSA_REF|{key}|{value:.17}");
}

fn print_vector(key: &str, value: DVec3) {
    println!(
        "SANSA_REF|{key}|{:.17},{:.17},{:.17}",
        value.x, value.y, value.z
    );
}

fn main() {
    let x = DVec3::X;
    let y = DVec3::Y;
    let z = DVec3::Z;
    let reference_gravity = DVec3::new(0.0, -9.80665, 0.0);

    print_vector("UBF-001.CANONICAL_X_M", x);
    print_scalar("UBF-002.CLOTH_WIDTH_M", 0.20);
    print_vector("CBF-001.BASIS_X", x);
    print_vector("CBF-001.BASIS_Y", y);
    print_vector("CBF-001.BASIS_Z", z);
    print_vector("CBF-003.NORMAL_Y", y);
    print_vector("CBF-004.REFERENCE_GRAVITY", reference_gravity);

    let plus90_z = DQuat::from_rotation_z(std::f64::consts::FRAC_PI_2);
    let minus90_z = DQuat::from_rotation_z(-std::f64::consts::FRAC_PI_2);
    print_vector("CBF-005.PLUS_90_Z_X", plus90_z * x);
    print_vector("CBF-005.MINUS_90_Z_X", minus90_z * x);

    let v00 = DVec3::new(-0.10, 0.0, -0.05);
    let v01 = DVec3::new(-0.10, 0.0, 0.05);
    let v11 = DVec3::new(0.10, 0.0, 0.05);
    let triangle_normal = (v01 - v00).cross(v11 - v00).normalize();
    print_vector("CBF-006.V00_V01_V11_NORMAL", triangle_normal);

    print_vector("CBF-007.CROSS_X_Y", x.cross(y));
    print_vector("CBF-007.CROSS_Y_Z", y.cross(z));
    print_vector("CBF-007.CROSS_Z_X", z.cross(x));

    let body_rotation = DQuat::from_rotation_z(std::f64::consts::FRAC_PI_2);
    print_vector("CBF-008.BODY_ROTATED_X", body_rotation * x);
    print_vector("CBF-008.WORLD_GRAVITY_UNCHANGED", reference_gravity);
    print_vector(
        "CBF-008.BODY_ROTATED_GRAVITY_REFERENCE",
        body_rotation * reference_gravity,
    );

    let surface_reference = SurfaceReference::new(1, 0.25, 0.75).unwrap();
    let identity_query = TransformedAnalyticFixtureSurfaceQuery {
        fixture: AnalyticFixture::flat(),
        transform: FixtureTransform::IDENTITY,
    };
    let identity_surface = identity_query.query(DVec3::ZERO, surface_reference);
    let identity_current =
        identity_surface.surface_position_m + identity_surface.surface_normal * 0.01;
    let identity_result = identity_query.query(identity_current, surface_reference);

    print_scalar("BF-003.DOMAIN_ID", surface_reference.domain_id as f64);
    print_scalar("BF-003.SURFACE_REFERENCE_U", surface_reference.u);
    print_scalar("BF-003.SURFACE_REFERENCE_V", surface_reference.v);
    print_vector(
        "BF-004.IDENTITY_SURFACE_POSITION",
        identity_result.surface_position_m,
    );
    print_vector(
        "BF-004.IDENTITY_SURFACE_NORMAL",
        identity_result.surface_normal,
    );
    print_scalar(
        "BF-004.IDENTITY_SEPARATION_M",
        identity_result.separation_m,
    );

    let transformed_query = TransformedAnalyticFixtureSurfaceQuery {
        fixture: AnalyticFixture::flat(),
        transform: FixtureTransform {
            rotation: DQuat::from_rotation_z(-std::f64::consts::FRAC_PI_2),
            translation_m: DVec3::new(0.30, 0.20, -0.10),
        },
    };
    let transformed_surface = transformed_query.query(DVec3::ZERO, surface_reference);
    let transformed_current =
        transformed_surface.surface_position_m + transformed_surface.surface_normal * 0.01;
    let transformed_result =
        transformed_query.query(transformed_current, surface_reference);

    print_scalar(
        "BF-003.TRANSFORMED_DOMAIN_ID",
        surface_reference.domain_id as f64,
    );
    print_scalar(
        "BF-003.TRANSFORMED_SURFACE_REFERENCE_U",
        surface_reference.u,
    );
    print_scalar(
        "BF-003.TRANSFORMED_SURFACE_REFERENCE_V",
        surface_reference.v,
    );
    print_vector(
        "BF-004.TRANSFORMED_SURFACE_POSITION",
        transformed_result.surface_position_m,
    );
    print_vector(
        "BF-004.TRANSFORMED_SURFACE_NORMAL",
        transformed_result.surface_normal,
    );
    print_scalar(
        "BF-004.TRANSFORMED_SEPARATION_M",
        transformed_result.separation_m,
    );
}
