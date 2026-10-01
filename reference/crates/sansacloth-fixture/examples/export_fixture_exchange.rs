use std::{env, fs, path::PathBuf};

use sansacloth_fixture::exchange::{
    BasicConformityCase, BasicExchangeScenario, BasicGravityCase, generate_basic_exchange,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let exchange = generate_basic_exchange(
        BasicExchangeScenario::Sr001Flat,
        BasicGravityCase::Off,
        BasicConformityCase::C0,
    );
    let json = exchange.to_json_pretty()?;

    let output = env::args_os().nth(1).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from("validation")
            .join("fixture-exchange")
            .join("SR-001-C0-G0.json")
    });

    if let Some(parent) = output.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(&output, json)?;

    println!("SANSA_FXE|CASE_ID|{}", exchange.case_id);
    println!(
        "SANSA_FXE|BODY_VERTEX_COUNT|{}",
        exchange.body_surface.vertices.len()
    );
    println!(
        "SANSA_FXE|BODY_TRIANGLE_COUNT|{}",
        exchange.body_surface.triangles.len()
    );
    println!("SANSA_FXE|CP_COUNT|{}", exchange.cloth.control_points.len());
    println!(
        "SANSA_FXE|ANCHOR_COUNT|{}",
        exchange
            .cloth
            .control_points
            .iter()
            .filter(|point| point.anchor)
            .count()
    );
    println!(
        "SANSA_FXE|CONTACT_INPUT_COUNT|{}",
        exchange
            .cloth
            .control_points
            .iter()
            .filter(|point| point.contact)
            .count()
    );
    println!("SANSA_FXE|OUTPUT|{}", output.display());

    Ok(())
}
