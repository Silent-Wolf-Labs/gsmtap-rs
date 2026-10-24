#[allow(dead_code)]
#[path = "support/bitvec_fixture.rs"]
mod bitvec_fixture;
#[path = "support/bitvec_buffers_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/bitvec/bitvec_buffers_vectors.json");

#[test]
fn bitvec_buffers_fixture_has_complete_c_observations() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}

#[test]
fn bitvec_buffers_validator_rejects_corrupt_records() {
    for corrupt in [
        FIXTURE.replacen("\"case\":0", "\"case\":1", 1),
        FIXTURE.replacen("789ef706", "00000000", 1),
        FIXTURE.replacen("\"num_bits\":0", "\"num_bits\":\"0\"", 1),
        FIXTURE.replacen(
            "\"word_hex\":\"0000000000000000\"",
            "\"word_hex\":\"0000000000000001\"",
            1,
        ),
        FIXTURE.replacen(
            "\"after_hex\":\"00ff2ba5ff800155\"",
            "\"after_hex\":\"00ff2ba5ff800154\"",
            1,
        ),
        FIXTURE.replacen("\"return_code\":0", "\"return_code\":2147483648", 1),
        FIXTURE.replacen("\"case\":0", "\"case\":0,\"case\":0", 1),
    ] {
        assert!(std::panic::catch_unwind(|| fixture::validated_records(&corrupt)).is_err());
    }
}
