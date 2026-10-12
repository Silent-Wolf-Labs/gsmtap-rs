#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_shared/fsm_shared_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_shared/fsm_shared_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_shared/fsm_shared_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_shared/fsm_shared_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0x92f87a17f7a23cd3,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0x0c8023a5e70531ed,
    generator_sha: "deb745bb8d428749e434e303c702abfac84782fd758753ea575690ae0c6d0ab5",
    diagnostics_hash: 0x3c276f92a7b77b76,
    records: 443,
    scenarios: 21,
    fields: 43,
    probes: 0,
};
#[test]
fn evidence_integrity_and_corruptions() {
    fsm_fixture::validate(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
    fsm_fixture::corruptions(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
