#[path = "support/time_fixture.rs"]
mod time_fixture;

const TEXT: &str = include_str!("vectors/time/time_vectors.ndjson");
const REFERENCE: &str = include_str!("vectors/time/time_provenance.json");
const GENERATOR: &str = include_str!("vectors/time/time_generator_provenance.json");

#[test]
fn time_evidence_integrity() {
    time_fixture::checks::integrity(TEXT, REFERENCE, GENERATOR);
}

#[test]
fn time_evidence_corruptions() {
    time_fixture::checks::corruptions(TEXT, REFERENCE, GENERATOR);
}
