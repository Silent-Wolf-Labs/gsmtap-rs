#[path = "support/gsm0503_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/gsm0503/xcch_vectors.ndjson");

#[test]
fn gsm0503_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn gsm0503_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
