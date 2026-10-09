#[path = "support/prim_fixture.rs"]
mod fixture;
const FIXTURE: &str = include_str!("vectors/prim/prim_vectors.json");

#[test]
fn validates_primitive_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(FIXTURE).len(), fixture::COUNT);
}

#[test]
fn rejects_corrupt_primitive_evidence() {
    fixture::check_corruptions(FIXTURE);
}
