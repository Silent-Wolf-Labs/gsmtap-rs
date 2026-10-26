#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_timer/fsm_timer_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_timer/fsm_timer_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_timer/fsm_timer_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_timer/fsm_timer_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0x521d233f523ba62a,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0x15b44fc0a588abf3,
    generator_sha: "2bf067670670ec52567f4c63a8f6fbbb2f06cad399fe49d766788a8ea0ed83d9",
    diagnostics_hash: 0xb3d1523434907a5b,
    records: 9369,
    scenarios: 1080,
    fields: 20,
    probes: 5,
};
#[test]
fn evidence_integrity_and_corruptions() {
    fsm_fixture::validate(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
    fsm_fixture::corruptions(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
