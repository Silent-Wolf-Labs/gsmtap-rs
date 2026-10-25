use std::path::PathBuf;

use serde_json::Value;

use super::*;

fn vectors() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/vectors")
}

/// Copies the named fixtures into a fresh directory and edits one record.
struct Corrupted(PathBuf);

impl Corrupted {
    fn new(name: &str, copies: &[&str], target: &str, edit: impl Fn(&mut Value) -> bool) -> Self {
        let dir =
            std::env::temp_dir().join(format!("gsmtap-conformance-{}-{name}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        for path in copies.iter().chain([&target]) {
            let destination = dir.join(path);
            std::fs::create_dir_all(destination.parent().unwrap()).unwrap();
            std::fs::copy(vectors().join(path), destination).unwrap();
        }
        let text = std::fs::read_to_string(dir.join(target)).unwrap();
        let edited = if let Ok(Value::Array(mut records)) = serde_json::from_str(&text) {
            assert!(records.iter_mut().any(&edit), "{target}: no record edited");
            serde_json::to_string(&records).unwrap()
        } else if let Ok(mut record @ Value::Object(_)) = serde_json::from_str(&text) {
            assert!(edit(&mut record), "{target}: record not edited");
            record.to_string()
        } else {
            let mut edited = false;
            let lines: Vec<String> = text
                .lines()
                .map(|line| {
                    let mut record: Value = serde_json::from_str(line).unwrap();
                    if !edited && edit(&mut record) {
                        edited = true;
                    }
                    record.to_string()
                })
                .collect();
            assert!(edited, "{target}: no line edited");
            lines.join("\n")
        };
        std::fs::write(dir.join(target), edited).unwrap();
        Self(dir)
    }

    fn run(&self, module: &str) -> ModuleReport {
        run(&self.0, &[module]).modules.remove(0)
    }
}

impl Drop for Corrupted {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

/// Replaces `key` in the first record satisfying `select`.
fn set(
    select: impl Fn(&Value) -> bool,
    key: &'static str,
    value: Value,
) -> impl Fn(&mut Value) -> bool {
    move |record| {
        if !select(record) {
            return false;
        }
        record[key] = value.clone();
        true
    }
}

fn is(key: &'static str, expected: &'static str) -> impl Fn(&Value) -> bool {
    move |record| record[key] == expected
}

fn assert_single_mismatch(report: &ModuleReport, field: &str) {
    assert_eq!(report.error, None);
    assert!(report.unsupported.is_empty(), "{:?}", report.unsupported);
    assert_eq!(report.mismatched.len(), 1, "{:?}", report.mismatched);
    let fields: Vec<_> = report.mismatched[0]
        .fields
        .iter()
        .map(|diff| diff.field.as_str())
        .collect();
    assert!(fields.contains(&field), "{fields:?}");
}

#[test]
fn selected_modules_run_alone() {
    let report = run(&vectors(), &["gsmtap", "hexparse"]);
    let modules: Vec<_> = report.modules.iter().map(|module| module.module).collect();
    assert_eq!(modules, ["hexparse", "gsmtap"]);
}

#[test]
fn corrupted_hexparse_return_is_reported() {
    let fixture = "hexparse/hexparse_buffer_vectors.json";
    let corrupted = Corrupted::new(
        "hexparse",
        &[],
        fixture,
        set(is("case", "lowercase"), "return_code", 9.into()),
    );
    let report = corrupted.run("hexparse");
    assert_single_mismatch(&report, "return_code");
    assert_eq!(report.mismatched[0].case, "lowercase");
    assert_eq!(report.mismatched[0].fields[0].c, "9");

    let summary = Report {
        osmocore_rs_version: OSMOCORE_RS_VERSION,
        modules: vec![report],
    }
    .to_string();
    assert!(
        summary.contains("hexparse: mismatch lowercase: return_code c=9 rust=4"),
        "{summary}"
    );
}

#[test]
fn corrupted_bcd_destination_is_reported() {
    let fixture = "bcd/bcd_conversion_vectors.json";
    let edit = set(
        is("api", "osmo_bcd2str"),
        "dst_after_hex",
        "0000000000".into(),
    );
    assert_single_mismatch(
        &Corrupted::new("bcd", &[], fixture, edit).run("bcd"),
        "dst_after_hex",
    );
}

#[test]
fn corrupted_bits_returns_are_reported() {
    let basic = "bits/bit_packing_vectors.json";
    let extended = "bits/bit_packing_ext_vectors.json";
    let edit = || set(|_| true, "return_code", 12_345.into());
    let report = Corrupted::new("bits-basic", &[extended], basic, edit()).run("bits");
    assert_single_mismatch(&report, "return_code");
    let report = Corrupted::new("bits-ext", &[basic], extended, edit()).run("bits");
    assert_single_mismatch(&report, "return_code");
}

#[test]
fn corrupted_base64_length_is_reported() {
    let fixture = "base64/base64_buffer_vectors.json";
    let edit = set(is("operation", "decode"), "olen", 77.into());
    assert_single_mismatch(
        &Corrupted::new("base64", &[], fixture, edit).run("base64"),
        "olen",
    );
}

#[test]
fn corrupted_crc_observations_are_reported() {
    let byte = "crc/crc_byte_vectors.json";
    let generic = "crc/crc_generic_vectors.json";
    let edit = set(is("kind", "buffer"), "mid", 1.into());
    let report = Corrupted::new("crc-byte", &[generic], byte, edit).run("crc");
    assert_single_mismatch(&report, "mid");
    let edit = set(
        |record| record["kind"] == "generic" && record["bits"] == 4,
        "flips",
        "00000000".into(),
    );
    let report = Corrupted::new("crc-generic", &[byte], generic, edit).run("crc");
    assert_single_mismatch(&report, "flips");
}

#[test]
fn corrupted_conv_decoder_state_is_reported() {
    let fixture = "conv/conv_vectors.ndjson";
    let edit = set(is("api", "decode_scan"), "history", "0;0;0;0".into());
    let report = Corrupted::new("conv-state", &[], fixture, edit).run("conv");
    assert_single_mismatch(&report, "history");
    let edit = set(is("api", "decode"), "output_after", "ff".into());
    let report = Corrupted::new("conv-output", &[], fixture, edit).run("conv");
    assert_single_mismatch(&report, "output_after");
}

#[test]
fn corrupted_gsmtap_packet_is_reported() {
    let fixture = "gsmtap_v2_basic_header.json";
    let edit = set(|_| true, "encoded_hex", "02".into());
    let report = Corrupted::new("gsmtap", &[], fixture, edit).run("gsmtap");
    assert_single_mismatch(&report, "encoded_hex");
}

#[test]
fn unknown_apis_are_unsupported_not_matched() {
    let fixture = "bcd/bcd_conversion_vectors.json";
    let edit = set(is("api", "osmo_bcd2str"), "api", "osmo_bcd2future".into());
    let report = Corrupted::new("unsupported", &[], fixture, edit).run("bcd");
    assert_eq!(report.unsupported.len(), 1);
    assert!(report.unsupported[0].reason.contains("osmo_bcd2future"));
    assert_eq!(report.matched, 75);

    let full = Report {
        osmocore_rs_version: OSMOCORE_RS_VERSION,
        modules: vec![report],
    };
    assert!(!full.passed());
    let summary = full.to_string();
    assert!(summary.contains("bcd: unsupported"), "{summary}");
    assert!(summary.ends_with("result: failed\n"), "{summary}");
}

#[test]
fn unreadable_fixtures_fail_the_module() {
    let empty =
        std::env::temp_dir().join(format!("gsmtap-conformance-{}-empty", std::process::id()));
    std::fs::create_dir_all(&empty).unwrap();
    let report = run(&empty, &[]);
    std::fs::remove_dir_all(&empty).unwrap();
    assert_eq!(report.errors(), MODULES.len());
    assert!(!report.passed());
}

#[test]
fn report_serializes_for_the_ui() {
    let report = run(&vectors(), &["gsmtap"]);
    let json = serde_json::to_value(&report).unwrap();
    assert_eq!(json["modules"][0]["module"], "gsmtap");
    assert_eq!(json["modules"][0]["matched"], 7);
    assert!(json["osmocoreRsVersion"].is_string());
}
