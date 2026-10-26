#[allow(dead_code)]
#[path = "support/fsm_fixture.rs"]
mod fsm_fixture;
const TEXT: &str = include_str!("vectors/fsm_shared/fsm_shared_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/fsm_shared/fsm_shared_provenance.json");
const GENERATOR: &str = include_str!("vectors/fsm_shared/fsm_shared_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/fsm_shared/fsm_shared_diagnostics.json");
const SPEC: fsm_fixture::Evidence = fsm_fixture::Evidence {
    text_hash: 0x796534f49a258c65,
    reference_hash: 0xcab6c5c42bd14581,
    reference_sha: "f572b2e98589d209217fdd17b117b79f1ad113b499a96898fd6c7c251d3d7fe0",
    generator_hash: 0x0a7265b3ecc40fb8,
    generator_sha: "9873b06cccd1fe1c179c5d71900963579986b8779d3ed44cc606d1a678568c57",
    diagnostics_hash: 0x85d765ec279ed564,
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
