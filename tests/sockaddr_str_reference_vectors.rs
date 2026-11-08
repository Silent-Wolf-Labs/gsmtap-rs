#[path = "support/sockaddr_str_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/sockaddr_str/sockaddr_str_vectors.ndjson");

#[test]
fn sockaddr_str_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn sockaddr_str_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
