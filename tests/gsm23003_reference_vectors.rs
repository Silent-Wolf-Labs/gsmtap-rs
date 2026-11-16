#[path = "support/gsm23003_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/gsm23003/gsm23003_vectors.ndjson");

#[test]
fn gsm23003_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn gsm23003_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
