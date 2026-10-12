#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_tree/fsm_tree_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_tree/fsm_tree_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_tree/fsm_tree_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_tree/fsm_tree_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0x45e754da7a75573b,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0x364e8bf385978b5b,
    generator_sha: "0c7f8098737ff00156fc20076f9447102a1f77eb4c97ec2f4c158152e6dbe8be",
    diagnostics_hash: 0x799ee9dea6929a28,
    records: 2010,
    scenarios: 134,
    fields: 43,
    probes: 5,
};
#[test]
fn evidence_integrity_and_corruptions() {
    fsm_fixture::validate(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
    fsm_fixture::corruptions(SPEC, TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
