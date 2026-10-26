#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_tree/fsm_tree_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_tree/fsm_tree_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_tree/fsm_tree_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_tree/fsm_tree_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0xd919a390c9a14184,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0xcd1e26364862a26a,
    generator_sha: "20b6ca3d2c09276476f69a22ca05893e75e96eee0d1888f39ea1c2dceb26bb5d",
    diagnostics_hash: 0xc4722a66858594e7,
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
