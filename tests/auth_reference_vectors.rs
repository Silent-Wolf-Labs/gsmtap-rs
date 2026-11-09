#[path = "support/auth_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/auth/auth_vectors.ndjson");

#[test]
fn auth_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn auth_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}

#[test]
fn auth_fixture_records_the_c_contract() {
    let records = fixture::validate(FIXTURE);
    let named = |algo: i128| {
        records
            .iter()
            .find(|record| record.kind == "alg_name" && record.int("algo") == algo)
            .map(|record| record.text("name"))
    };
    assert_eq!(named(5), Some("MILENAGE"));
    assert_eq!(named(8), Some("unknown 0x8"));
    for record in records.iter().filter(|r| r.kind.starts_with("gen_vec")) {
        // A GSM subscriber's record ends with its Ki, any other with its SQNs.
        let gsm = record.int("type") == 1;
        assert_eq!(record.has("out_key"), gsm);
        assert_eq!(record.has("out_sqn"), !gsm);
        // No algorithm, no implementation.
        if record.int("algo") == 0 {
            assert_eq!(record.int("rc"), fixture::ENOENT);
        }
    }
    // Test set 1's OPc, as 3GPP TS 35.207 gives it.
    let opc = records.iter().find(|r| r.kind == "opc").unwrap();
    assert_eq!(
        opc.array::<16>("opc"),
        [
            0xcd, 0x63, 0xcb, 0x71, 0x95, 0x4a, 0x9f, 0x4e, 0x48, 0xa5, 0x99, 0x4e, 0x37, 0xa0,
            0x2b, 0xaf
        ]
    );
}
