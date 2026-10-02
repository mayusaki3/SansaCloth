use std::{collections::HashSet, env, fs, path::PathBuf};

use sansacloth_fixture::exchange::{
    BASIC_EXCHANGE_CASE_COUNT, FixtureExchange, generate_basic_exchange_matrix,
};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let output_dir = env::args_os().nth(1).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from("validation")
            .join("fixture-exchange")
            .join("basic-v1")
    });

    fs::create_dir_all(&output_dir)?;

    let exchanges = generate_basic_exchange_matrix();
    if exchanges.len() != BASIC_EXCHANGE_CASE_COUNT {
        return Err(format!(
            "unexpected exchange matrix size: {} != {}",
            exchanges.len(),
            BASIC_EXCHANGE_CASE_COUNT
        )
        .into());
    }

    let mut case_ids = HashSet::with_capacity(BASIC_EXCHANGE_CASE_COUNT);
    let mut written = 0usize;
    for exchange in &exchanges {
        if !case_ids.insert(exchange.case_id.clone()) {
            return Err(format!("duplicate matrix case_id: {}", exchange.case_id).into());
        }
        write_and_verify(&output_dir, exchange)?;
        written += 1;
    }

    println!("SANSA_FXE|MATRIX_CASE_COUNT|{}", exchanges.len());
    println!("SANSA_FXE|MATRIX_WRITTEN_COUNT|{written}");
    println!("SANSA_FXE|MATRIX_ROUND_TRIP_COUNT|{written}");
    println!("SANSA_FXE|MATRIX_OUTPUT_DIR|{}", output_dir.display());

    Ok(())
}

fn write_and_verify(
    output_dir: &std::path::Path,
    exchange: &FixtureExchange,
) -> Result<(), Box<dyn std::error::Error>> {
    let json = exchange.to_json_pretty()?;
    let output = output_dir.join(format!("{}.json", exchange.case_id));
    fs::write(&output, &json)?;

    let written_json = fs::read_to_string(&output)?;
    let imported = FixtureExchange::from_json(&written_json)?;
    if !imported.semantically_equivalent(exchange) {
        return Err(format!("round-trip semantic mismatch for {}", exchange.case_id).into());
    }

    println!("SANSA_FXE|MATRIX_CASE|{}|PASS", exchange.case_id);
    println!("SANSA_FXE|MATRIX_OUTPUT|{}", output.display());
    Ok(())
}
