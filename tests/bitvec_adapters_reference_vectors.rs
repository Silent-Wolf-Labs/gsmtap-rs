#[path = "support/bitvec_adapters_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/bitvec/bitvec_adapters_vectors.json");

#[test]
fn bitvec_adapters_fixture_has_complete_c_observations() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}

#[test]
fn bitvec_adapters_validator_rejects_corrupt_records() {
    for corrupt in [
        FIXTURE.replacen("\"case\":0", "\"case\":1", 1),
        FIXTURE.replacen("a5d9d5dc", "00000000", 1),
        FIXTURE.replacen("\"io_len\":0", "\"io_len\":\"0\"", 1),
        FIXTURE.replacen("\"src_hex\":\"\"", "\"src_hex\":\"00\"", 1),
        FIXTURE.replacen("\"input\":\"none\"", "\"input\":\"unexpected\"", 1),
        FIXTURE.replacen("\"case\":0", "\"case\":0,\"case\":0", 1),
        FIXTURE.replacen("\"cursor_after\":0", "\"cursor_after\":4294967296", 1),
    ] {
        assert!(std::panic::catch_unwind(|| fixture::validated_records(&corrupt)).is_err());
    }
}
