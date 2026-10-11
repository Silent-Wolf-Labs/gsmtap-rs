#[path = "support/tdef_fixture.rs"]
mod tdef_fixture;
const TEXT: &str = include_str!("vectors/tdef/tdef_vectors.ndjson");
const REFERENCE: &str = include_str!("vectors/tdef/tdef_provenance.json");
const GENERATOR: &str = include_str!("vectors/tdef/tdef_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/tdef/tdef_diagnostics.json");
#[test]
fn tdef_evidence_integrity() {
    tdef_fixture::checks::integrity(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
#[test]
fn tdef_evidence_corruptions() {
    tdef_fixture::checks::corruptions(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
