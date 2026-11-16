#[path = "support/osmo_auc_gen_fixture.rs"]
mod fixture;

const FIXTURE: &str = include_str!("vectors/osmo-auc-gen/osmo_auc_gen_vectors.ndjson");

fn runs() -> Vec<fixture::Run> {
    fixture::validate(FIXTURE)
}

fn run<'a>(runs: &'a [fixture::Run], name: &str) -> &'a fixture::Run {
    runs.iter()
        .find(|run| run.name == name)
        .unwrap_or_else(|| panic!("no run {name}"))
}

fn line<'a>(run: &'a fixture::Run, label: &str) -> &'a str {
    let prefix = format!("{label}:\t");
    run.body()
        .lines()
        .find_map(|line| line.strip_prefix(prefix.as_str()))
        .unwrap_or_else(|| panic!("{} has no {label}", run.name))
}

#[test]
fn osmo_auc_gen_fixture_has_the_recorded_c_observations() {
    let total: usize = fixture::SECTIONS.iter().map(|(_, count)| count).sum();
    assert_eq!(runs().len(), total);
}

#[test]
fn osmo_auc_gen_fixture_rejects_corruption() {
    fixture::check_corruptions(FIXTURE);
}

#[test]
fn osmo_auc_gen_fixture_records_the_c_contract() {
    let runs = runs();
    // Only the upstream script's four error commands fail, and they fail on IND.
    let failed: Vec<_> = runs
        .iter()
        .filter(|run| run.source == "upstream" && run.status != 0)
        .map(|run| run.name.as_str())
        .collect();
    assert_eq!(
        failed,
        [
            "auts-ind-minus-1",
            "auts-ind-32",
            "auts-ind-42",
            "auts-ind-len-0-ind-1"
        ]
    );
    // A failed run prints nothing after the banner, except where it printed help.
    for run in runs.iter().filter(|run| run.status != 0) {
        let help = run.stderr.contains("\n  MILENAGE\n");
        assert_eq!(run.body().is_empty(), !help, "{}", run.name);
    }
    // SQN is parsed like strtoull with base 0: -1, the largest value and an overflow all
    // give 2^64 - 1, text gives 0, and 2^48 gives the same AUTN as 0.
    let autn = |name| line(run(&runs, name), "AUTN");
    assert_eq!(autn("milenage-sqn-minus-1"), autn("milenage-sqn-u64-max"));
    assert_eq!(autn("milenage-sqn-overflow"), autn("milenage-sqn-u64-max"));
    assert_eq!(autn("milenage-sqn-text"), autn("milenage-sqn-2-48"));
    assert_eq!(autn("milenage-sqn-hex"), autn("milenage-default-sqn"));
    assert_eq!(line(run(&runs, "milenage-sqn-octal"), "SQN"), "8");
    assert_eq!(line(run(&runs, "milenage-sqn-trailing"), "SQN"), "12");
    // Without --sqn, the first SQN is the next after 0 for IND 0: 32 with five IND bits.
    assert_eq!(line(run(&runs, "milenage-default-sqn"), "SQN"), "32");
    // XOR-3G uses SQN as given, so the SQN printed is one IND step before the one asked.
    assert_eq!(line(run(&runs, "xor-3g-sqn"), "SQN"), "68");
    // A resync prints the USIM's SQN.
    assert_eq!(line(run(&runs, "tuak-auts"), "SQN.MS"), "31");
    assert_eq!(line(run(&runs, "auts"), "SQN.MS"), "23");
    // Abbreviated long options and grouped short options give the same SQN 5 vector.
    for label in ["AUTN", "SQN"] {
        assert_eq!(
            line(run(&runs, "long-abbreviated"), label),
            line(run(&runs, "short-grouped"), label)
        );
    }
    assert_eq!(line(run(&runs, "long-abbreviated"), "RES").len(), 8);
    // A key given before -2 or -3 is dropped, so XOR-2G gives RAND itself.
    let dropped = run(&runs, "gsm-key-before-type");
    assert_eq!(line(dropped, "SRES"), &line(dropped, "RAND")[..8]);
}
