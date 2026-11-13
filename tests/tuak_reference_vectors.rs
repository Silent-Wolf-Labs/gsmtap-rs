#[path = "support/tuak_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/tuak/tuak_vectors.ndjson");

#[test]
fn tuak_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(fixture::validate(FIXTURE).len(), total);
}

#[test]
fn tuak_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}

#[test]
fn tuak_fixture_records_the_c_contract() {
    let records = fixture::validate(FIXTURE);
    let of = |kind: &'static str| records.iter().filter(move |r| r.kind == kind);
    // f1 and f1* differ only in the instance byte, which changes every MAC.
    let upstream_macs: Vec<_> = of("f1")
        .filter(|r| r.text("source") == "upstream")
        .map(|r| (r.flag("star"), r.written("mac").to_vec()))
        .collect();
    for pair in upstream_macs.chunks(2) {
        assert_eq!((pair[0].0, pair[1].0), (false, true));
        assert_ne!(pair[0].1, pair[1].1);
    }
    // C checks AUTS with all 32 bytes of the key buffer: with a 16-byte K, the AUTS
    // the harness made from the 16 bytes alone fails and the one from all 32 passes.
    let short_key: Vec<i128> = of("gen_vec_auts")
        .filter(|r| r.int("k_len") == 16 && r.int("opc_is_op") == 0)
        .map(|r| r.int("rc"))
        .collect();
    assert_eq!(short_key, [-1, 0, -1, 0]);
    // A vector carries its RAND.
    let vector = of("gen_vec").find(|r| r.int("rc") == 0).expect("a vector");
    assert_eq!(vector.array::<16>("out_rand"), vector.array::<16>("rand"));
}
