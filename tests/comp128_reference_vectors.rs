#[path = "support/comp128_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/comp128/comp128_vectors.ndjson");

#[test]
fn comp128_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn comp128_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
