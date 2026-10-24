#[path = "support/msgb_layers_fixture.rs"]
mod fixture;
#[allow(dead_code)]
#[path = "support/msgb_fixture.rs"]
mod msgb_fixture;
const FIXTURE: &str = include_str!("vectors/msgb/msgb_layers_vectors.json");
#[test]
fn validates_typed_layer_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}
#[test]
fn rejects_corrupt_typed_and_layer_evidence() {
    fixture::check_corruptions(FIXTURE);
}
