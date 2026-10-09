#[path = "support/msgb_adapters_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/msgb/msgb_adapters_vectors.json");
#[test]
fn validates_remaining_adapter_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}
#[test]
fn rejects_corrupt_adapter_evidence() {
    fixture::check_corruptions(FIXTURE);
}
