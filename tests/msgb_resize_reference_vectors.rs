#[path = "support/gzip_fixture.rs"]
mod gzip_fixture;
use std::sync::LazyLock;
#[path = "support/msgb_resize_fixture.rs"]
mod fixture;
#[allow(dead_code)]
#[path = "support/msgb_fixture.rs"]
mod msgb_fixture;
static FIXTURE: LazyLock<String> = LazyLock::new(|| {
    gzip_fixture::text(include_bytes!("vectors/msgb/msgb_resize_vectors.json.gz"))
});
#[test]
fn validates_copy_resize_observations_and_frozen_coverage() {
    assert_eq!(fixture::validated_records(&FIXTURE).len(), fixture::COUNT);
}
#[test]
fn rejects_corrupt_copy_resize_evidence() {
    fixture::check_corruptions(&FIXTURE);
}
