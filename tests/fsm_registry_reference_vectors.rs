#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_registry/fsm_registry_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_registry/fsm_registry_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_registry/fsm_registry_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_registry/fsm_registry_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0xef5b4067fa748c54,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0xd24737b6058e5414,
    generator_sha: "06703afdfe07650cc4b40a99226d240137c37d8194d8503e56a6cff64c29d99f",
    diagnostics_hash: 0xe14d675fde7e2a6a,
    records: 1524,
    scenarios: 127,
    fields: 32,
    probes: 2,
};
#[test]
fn evidence_integrity_and_corruptions() {
    fsm_fixture::validate(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
    fsm_fixture::corruptions(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
