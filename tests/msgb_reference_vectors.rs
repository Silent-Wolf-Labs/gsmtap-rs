#[path = "support/msgb_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/msgb/msgb_vectors.json");
#[test]
fn validates_complete_c_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}
#[test]
fn rejects_corrupt_schema_provenance_and_observations() {
    fixture::check_corruptions(FIXTURE);
}
