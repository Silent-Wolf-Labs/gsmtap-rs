#[path = "support/gsmtap_constants_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/gsmtap_constants/gsmtap_constants.ndjson");

#[test]
fn gsmtap_constants_fixture_has_the_recorded_c_observations() {
    assert_eq!(fixture::validate(FIXTURE).len(), fixture::CONSTANT_COUNT);
}

#[test]
fn gsmtap_constants_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
