#[path = "support/crc_fixture.rs"]
mod fixture;
const BYTE: &str = include_str!("vectors/crc/crc_byte_vectors.json");
const GENERIC: &str = include_str!("vectors/crc/crc_generic_vectors.json");

#[test]
fn validates_crc_observations_and_frozen_coverage() {
    assert_eq!(fixture::validate_byte(BYTE).len(), fixture::BYTE_COUNT);
    assert_eq!(
        fixture::validate_generic(GENERIC).len(),
        fixture::GENERIC_COUNT
    );
}

#[test]
fn rejects_corrupt_crc_evidence() {
    fixture::check_corruptions(BYTE, false);
    fixture::check_corruptions(GENERIC, true);
}
