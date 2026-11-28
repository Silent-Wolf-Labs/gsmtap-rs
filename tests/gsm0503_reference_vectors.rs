#[path = "support/gsm0503_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/gsm0503/xcch_vectors.ndjson");

#[test]
fn gsm0503_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn gsm0503_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}

#[path = "support/gsm0503_codes_fixture.rs"]
mod codes_fixture;

const CODES_FIXTURE: &str = include_str!("vectors/gsm0503/codes_vectors.ndjson");

#[test]
fn gsm0503_codes_fixture_has_the_recorded_c_observations() {
    let per_code = 1 + codes_fixture::CODE_CASES.len();
    let per_crc = 1 + codes_fixture::CRC_CASES.len();
    let total =
        codes_fixture::UPSTREAM_LENGTHS.len() * per_code + codes_fixture::CRCS.len() * per_crc;
    assert_eq!(codes_fixture::validate(CODES_FIXTURE).len(), total);
}

#[test]
fn gsm0503_codes_fixture_rejects_corruption() {
    codes_fixture::check_corruptions(CODES_FIXTURE);
}

#[path = "support/gsm0503_tch_fr_fixture.rs"]
mod tch_fr_fixture;

const TCH_FR_FIXTURE: &str = include_str!("vectors/gsm0503/tch_fr_vectors.ndjson");

#[test]
fn gsm0503_tch_fr_fixture_has_the_recorded_c_observations() {
    let total: usize = tch_fr_fixture::SECTIONS
        .iter()
        .map(|(_, count)| count)
        .sum();
    assert_eq!(tch_fr_fixture::validate(TCH_FR_FIXTURE).len(), total);
}

#[test]
fn gsm0503_tch_fr_fixture_rejects_corruption() {
    tch_fr_fixture::check_corruptions(TCH_FR_FIXTURE);
}
