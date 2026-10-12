#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_core/fsm_core_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_core/fsm_core_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_core/fsm_core_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_core/fsm_core_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0xc800eca23162bfac,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0xd80aa50df9f8b7fc,
    generator_sha: "7c21fa06c2a084af8a38eac787ff2ef43fc2815c26ed17fa23f6654c2c5a54e8",
    diagnostics_hash: 0x4ca0415a998b1987,
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
