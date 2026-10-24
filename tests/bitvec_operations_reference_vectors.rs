#[allow(dead_code)]
#[path = "support/bitvec_fixture.rs"]
mod bitvec_fixture;
#[path = "support/bitvec_operations_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/bitvec/bitvec_operations_vectors.json");

#[test]
fn bitvec_operations_fixture_has_complete_c_observations() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}

#[test]
fn bitvec_operations_validator_rejects_corrupt_records() {
    for corrupt in [
        FIXTURE.replacen("\"case\":0", "\"case\":1", 1),
        FIXTURE.replacen("789ef706", "00000000", 1),
        FIXTURE.replacen("\"argument\":0", "\"argument\":\"0\"", 1),
        FIXTURE.replacen(
            "\"array_hex\":\"00000000ffffffff8000000012345678\"",
            "\"array_hex\":\"00000000ffffffff8000000012345679\"",
            1,
        ),
        FIXTURE.replacen("\"errno_after\":0", "\"errno_after\":1", 1),
        FIXTURE.replacen(
            "\"after_hex\":\"0000ff2ba500ff80\"",
            "\"after_hex\":\"0000ff2ba500ff81\"",
            1,
        ),
        FIXTURE.replacen("\"index_after\":0", "\"index_after\":4294967296", 1),
        FIXTURE.replacen("\"case\":0", "\"case\":0,\"case\":0", 1),
    ] {
        assert!(std::panic::catch_unwind(|| fixture::validated_records(&corrupt)).is_err());
    }
}
