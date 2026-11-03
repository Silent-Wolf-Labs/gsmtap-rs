#[path = "support/use_count_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/use_count/use_count_vectors.ndjson");

#[test]
fn use_count_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn use_count_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
