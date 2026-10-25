//! Replays every committed fixture through osmocore-rs and writes the report.
//!
//! The report is written before any assertion, so a failing run still leaves
//! `conformance-report.json` and `conformance-report.txt` in `target/tmp/`.
#![cfg(feature = "osmocore")]

use std::path::Path;

use gsmtap_rs::conformance;

const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";

#[test]
fn every_committed_case_matches() {
    let vectors = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/vectors");
    let report = conformance::run(&vectors, &[]);

    let out = Path::new(env!("CARGO_TARGET_TMPDIR"));
    let json = serde_json::to_string_pretty(&report).unwrap();
    std::fs::write(out.join("conformance-report.json"), json).unwrap();
    std::fs::write(out.join("conformance-report.txt"), report.to_string()).unwrap();
    eprintln!("conformance report written to {}", out.display());

    assert!(report.passed(), "{report}");
    let summary: Vec<_> = report
        .modules
        .iter()
        .map(|module| (module.module, module.cases(), module.matched))
        .collect();
    assert_eq!(
        summary,
        [
            ("hexparse", 21, 21),
            ("bcd", 76, 76),
            ("bits", 72_400, 72_400),
            ("base64", 24, 24),
            ("crc", 26_320, 26_320),
            ("conv", 35_186, 35_186),
            ("gsmtap", 7, 7),
        ]
    );
    assert!(report.osmocore_rs_version.starts_with("0."));
    for module in &report.modules {
        for fixture in &module.fixtures {
            assert_eq!(fixture.sha256.len(), 64);
            let expected = if module.module == "conv" {
                "unknown"
            } else {
                LIBOSMOCORE_COMMIT
            };
            assert_eq!(fixture.libosmocore_revision, expected, "{}", fixture.path);
        }
    }
}
