#[path = "support/bitvec_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/bitvec/bitvec_vectors.json");

#[test]
fn bitvec_fixture_has_complete_c_observations() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}

#[test]
fn bitvec_validator_rejects_corrupt_records() {
    for corrupt in [
        FIXTURE.replacen("\"case\":0", "\"case\":1", 1),
        FIXTURE.replacen("\"position\":0", "\"position\":\"0\"", 1),
        FIXTURE.replacen("789ef706", "00000000", 1),
        FIXTURE.replacen(
            "\"before_hex\":\"00ff2ba5\"",
            "\"before_hex\":\"00ff2ba4\"",
            1,
        ),
        FIXTURE.replacen(
            "\"after_hex\":\"00ff2ba5\"",
            "\"after_hex\":\"00ff2ba4\"",
            1,
        ),
        FIXTURE.replacen("\"api\":", "\"unexpected\":", 1),
        FIXTURE.replacen("\"case\":0", "\"case\":0,\"case\":0", 1),
    ] {
        assert!(std::panic::catch_unwind(|| fixture::validated_records(&corrupt)).is_err());
    }
}
