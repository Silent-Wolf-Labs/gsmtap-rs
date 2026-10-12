#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_timer/fsm_timer_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_timer/fsm_timer_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_timer/fsm_timer_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_timer/fsm_timer_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0x718ddf70ddab0c2a,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0xab31fa87b6541e57,
    generator_sha: "ef14c849c7c4b4c51e934bdff371aea361dc018bb82d95f6559a377a1937ec5d",
    diagnostics_hash: 0x6e0ffb238c8464cf,
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
