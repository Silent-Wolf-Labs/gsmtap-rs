#[path = "support/prbs_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/prbs/prbs_vectors.ndjson");

#[test]
fn prbs_fixture_has_the_recorded_c_observations() {
    assert_eq!(fixture::validate(FIXTURE).len(), fixture::RECORD_COUNT);
}

#[test]
fn prbs_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
