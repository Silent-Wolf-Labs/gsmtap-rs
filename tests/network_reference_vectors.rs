//! Normalized C socket-establishment observations; no traffic/packet loader use.
#[path = "support/network_fixture.rs"]
mod fixture;
const VECTORS: &str = include_str!("vectors/net/network_vectors.tsv");
#[test]
fn network_observations_have_complete_bounded_coverage() {
    let rows = fixture::parse(VECTORS).unwrap();
    assert_eq!(rows.len(), 6);
    assert!(fixture::parse(&VECTORS.replace("udp_configured", "udp_bound")).is_err());
    assert!(fixture::parse(&VECTORS.replace("\t46\t", "\t64\t")).is_err());
    assert!(fixture::parse(VECTORS.trim_end()).is_err());
    assert!(fixture::parse(&format!("{VECTORS}extra\n")).is_err());
    assert!(fixture::parse(&VECTORS.replace("\t46\t", "\t+46\t")).is_err());
}
