#[path = "support/time_cc_fixture.rs"]
mod time_cc_fixture;
const TEXT: &str = include_str!("vectors/time_cc/time_cc_vectors.ndjson");
const REFERENCE: &str = include_str!("vectors/time_cc/time_cc_provenance.json");
const GENERATOR: &str = include_str!("vectors/time_cc/time_cc_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/time_cc/time_cc_diagnostics.json");
#[test]
fn time_cc_evidence_integrity() {
    time_cc_fixture::checks::integrity(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
#[test]
fn time_cc_evidence_corruptions() {
    time_cc_fixture::checks::corruptions(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
