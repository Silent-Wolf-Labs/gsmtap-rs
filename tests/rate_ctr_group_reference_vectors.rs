#[allow(dead_code)]
#[path = "support/rate_ctr_group_fixture.rs"]
mod fixture;
const TEXT: &str = include_str!("vectors/rate_ctr_group/rate_ctr_group_vectors.tsv");
const REFERENCE: &str = include_str!("vectors/rate_ctr_group/rate_ctr_group_provenance.json");
const GENERATOR: &str =
    include_str!("vectors/rate_ctr_group/rate_ctr_group_generator_provenance.json");
const DIAGNOSTICS: &str = include_str!("vectors/rate_ctr_group/rate_ctr_group_diagnostics.json");
const NATIVE: &str = include_str!("vectors/rate_ctr_group/rate_ctr_group_native.tsv");
#[test]
fn stage7_evidence_integrity() {
    fixture::integrity(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS, NATIVE);
}
#[test]
fn stage7_evidence_corruptions() {
    fixture::corruptions(TEXT, REFERENCE, GENERATOR, DIAGNOSTICS, NATIVE);
}
