#[path = "support/kdf_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/kdf/kdf_vectors.ndjson");

#[test]
fn kdf_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn kdf_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}

#[test]
fn kdf_fixture_records_the_c_contract() {
    let records = fixture::validate(FIXTURE);
    let of = |kind: &'static str| records.iter().filter(move |r| r.kind == kind);
    // Only SQN xor AK enters K_ASME: the harness's three ways of concealing one SQN give
    // one key, and an SQN equal to its AK gives the key of SQN and AK both zero.
    let concealed: Vec<[u8; 32]> = of("kasme").skip(2).map(|r| r.output()).collect();
    assert_eq!(concealed[0], concealed[2]);
    assert_eq!(concealed[0], concealed[3]);
    assert_eq!(concealed[0], concealed[4]);
    assert_ne!(concealed[0], concealed[1]);
    assert_ne!(concealed[0], concealed[5]);
    // The NH chain: each sync input is the output before it, and the first is K_eNB.
    let kenb: [u8; 32] = of("enb")
        .find(|r| r.int("ul_count") == 0 && r.bytes("kasme")[1] == 1)
        .expect("K_eNB for count 0")
        .output();
    let chain: Vec<_> = of("nh").filter(|r| r.text("source") == "chain").collect();
    assert_eq!(chain.len(), 4);
    assert_eq!(chain[0].array::<32>("sync_input"), kenb);
    for pair in chain.windows(2) {
        assert_eq!(pair[1].array::<32>("sync_input"), pair[0].output::<32>());
    }
    // Every algorithm type and identity gives its own key.
    let mut sweep: Vec<[u8; 16]> = of("nas")
        .filter(|r| r.text("source") == "sweep")
        .map(|r| r.output())
        .collect();
    let swept = sweep.len();
    sweep.sort_unstable();
    sweep.dedup();
    assert_eq!((swept, sweep.len()), (128, 128));
}
