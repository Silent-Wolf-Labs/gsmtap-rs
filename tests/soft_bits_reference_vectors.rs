#[path = "support/soft_bits_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/bits/soft_bits.ndjson");

#[test]
fn soft_bits_fixture_has_the_recorded_c_observations() {
    let recorded = fixture::validate(FIXTURE);
    assert_eq!(
        (recorded.ubit2sbit.len(), recorded.sbit2ubit.len()),
        (256, 256)
    );
}

#[test]
fn soft_bits_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}
