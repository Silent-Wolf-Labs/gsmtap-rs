#[path = "support/gzip_fixture.rs"]
mod gzip_fixture;
use std::sync::LazyLock;
#[path = "support/msgb_fixture.rs"]
mod fixture;
static FIXTURE: LazyLock<String> =
    LazyLock::new(|| gzip_fixture::text(include_bytes!("vectors/msgb/msgb_vectors.json.gz")));
#[test]
fn validates_complete_c_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(&FIXTURE).len(), fixture::COUNT);
}
#[test]
fn rejects_corrupt_schema_provenance_and_observations() {
    fixture::check_corruptions(&FIXTURE);
}
