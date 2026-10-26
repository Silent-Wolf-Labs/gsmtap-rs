#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_core/fsm_core_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_core/fsm_core_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_core/fsm_core_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_core/fsm_core_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0xbbde5a5c6b3dc6c2,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0x2d92181512c078c7,
    generator_sha: "9f5c41d6e60b9999ee511de720b6abd66eacf5c70a9b09a1f22436325bce38bd",
    diagnostics_hash: 0xb7a8b2b8e70d7d35,
    records: 1506,
    scenarios: 64,
    fields: 14,
    probes: 7,
};
#[test]
fn evidence_integrity_and_corruptions() {
    fsm_fixture::validate(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
    fsm_fixture::corruptions(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
