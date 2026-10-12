#[allow(dead_code)]
#[path = "support/rate_ctr_fixture.rs"]
mod rate_ctr_fixture;
const TEXT: &str = include_str!("vectors/rate_ctr/rate_ctr_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/rate_ctr/rate_ctr_provenance.json");
const GENERATOR: &str = include_str!("vectors/rate_ctr/rate_ctr_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/rate_ctr/rate_ctr_diagnostics.json");
#[test]
fn rate_ctr_evidence_integrity() {
    rate_ctr_fixture::checks::integrity(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
#[test]
fn rate_ctr_evidence_corruptions() {
    rate_ctr_fixture::checks::corruptions(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS);
}
