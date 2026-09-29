use std::collections::BTreeSet;

use serde_json::{Map, Value};

const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
const BUFFER_CASES: &[&str] = &[
    "decode_low_first",
    "decode_odd_start",
    "decode_hex_allowed",
    "decode_hex_disallowed",
    "decode_filler_disallowed",
    "decode_exact_fit",
    "decode_oversized",
    "decode_truncated",
    "decode_invalid_after_truncation",
    "decode_invalid_before_truncation",
    "decode_one_byte",
    "decode_zero_capacity",
    "decode_empty_range",
    "decode_inverted_range",
    "decode_last_nibble",
    "encode_auto_even",
    "encode_auto_odd_filler",
    "encode_prefix_auto_filler",
    "encode_explicit_filler",
    "encode_explicit_odd_end",
    "encode_empty_range",
    "encode_inverted_range",
    "encode_upper_hex",
    "encode_lower_hex",
    "encode_hex_disallowed_first",
    "encode_hex_disallowed_after_write",
    "encode_invalid_first",
    "encode_invalid_after_write",
    "encode_embedded_nul",
    "encode_exact_fit",
    "encode_oversized",
    "encode_too_small",
    "encode_zero_capacity",
    "encode_last_nibble",
    "encode_empty_auto",
];

fn required<'a>(record: &'a Map<String, Value>, field: &str) -> &'a Value {
    record
        .get(field)
        .unwrap_or_else(|| panic!("BCD fixture is missing {field}"))
}

fn bytes(record: &Map<String, Value>, field: &str) -> Vec<u8> {
    let hex = required(record, field)
        .as_str()
        .unwrap_or_else(|| panic!("{field} must be a hex string"));
    assert_eq!(hex.len() % 2, 0, "{field} has an odd hex length");
    assert!(
        hex.bytes()
            .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte)),
        "{field} must contain lowercase hexadecimal bytes"
    );
    (0..hex.len())
        .step_by(2)
        .map(|index| u8::from_str_radix(&hex[index..index + 2], 16).unwrap())
        .collect()
}

fn expected_cases() -> BTreeSet<String> {
    let mut cases: BTreeSet<String> = BUFFER_CASES.iter().map(ToString::to_string).collect();
    for digit in 0..16 {
        cases.insert(format!("bcd2char_{digit:x}"));
        cases.insert(format!("char2bcd_{digit:x}"));
    }
    for digit in 10..16 {
        cases.insert(format!("char2bcd_lower_{digit:x}"));
    }
    for name in ["char2bcd_invalid", "char2bcd_nul", "char2bcd_high_bit"] {
        cases.insert(name.to_owned());
    }
    assert_eq!(cases.len(), 76);
    cases
}

#[test]
fn bcd_fixture_has_complete_c_observations() {
    let fixture = include_str!("vectors/bcd/bcd_conversion_vectors.json");
    let records: Value = serde_json::from_str(fixture).expect("BCD fixture must be valid JSON");
    let records = records.as_array().expect("BCD fixture must be an array");
    assert_eq!(records.len(), 76);

    let mut observed_cases = BTreeSet::new();
    for record in records {
        let record = record.as_object().expect("each BCD case must be an object");
        let case = required(record, "case")
            .as_str()
            .expect("case must be a string");
        assert!(
            observed_cases.insert(case.to_owned()),
            "duplicate case: {case}"
        );
        assert_eq!(
            required(record, "libosmocore_commit").as_str(),
            Some(LIBOSMOCORE_COMMIT),
            "{case} has unexpected C provenance"
        );

        let api = required(record, "api")
            .as_str()
            .expect("api must be a string");
        let source = bytes(record, "src_hex");
        let return_code = required(record, "return_code")
            .as_i64()
            .expect("return_code must be a signed integer");
        assert!(
            i32::try_from(return_code).is_ok(),
            "{case} return_code exceeds C int"
        );

        let scalar = case.starts_with("bcd2char_") || case.starts_with("char2bcd_");
        let expected_api = if case.starts_with("bcd2char_") {
            "osmo_bcd2char"
        } else if case.starts_with("char2bcd_") {
            "osmo_char2bcd"
        } else if case.starts_with("decode_") {
            "osmo_bcd2str"
        } else if case.starts_with("encode_") {
            "osmo_str2bcd"
        } else {
            panic!("unknown BCD case: {case}");
        };
        assert_eq!(api, expected_api, "{case} API");

        let mut expected_fields: BTreeSet<&str> = [
            "api",
            "case",
            "libosmocore_commit",
            "return_code",
            "src_hex",
        ]
        .into_iter()
        .collect();
        if scalar {
            assert_eq!(source.len(), 1, "{case} must have one input byte");
            if api == "osmo_bcd2char" {
                assert!((48..=57).contains(&return_code) || (65..=70).contains(&return_code));
            } else {
                assert!((0..=15).contains(&return_code));
            }
        } else {
            expected_fields.extend([
                "allow_hex",
                "dst_after_hex",
                "dst_before_hex",
                "dst_len",
                "end_nibble",
                "start_nibble",
            ]);
            assert!(
                required(record, "allow_hex").is_boolean(),
                "{case} allow_hex must be boolean"
            );
            let dst_len = required(record, "dst_len")
                .as_u64()
                .expect("dst_len must be a nonnegative integer");
            let dst_len = usize::try_from(dst_len).expect("dst_len must fit usize");
            let before = bytes(record, "dst_before_hex");
            let after = bytes(record, "dst_after_hex");
            assert_eq!(before.len(), dst_len, "{case} initial destination length");
            assert_eq!(after.len(), dst_len, "{case} final destination length");
            for (index, byte) in before.iter().enumerate() {
                assert_eq!(*byte, 0xa5 ^ index as u8, "{case} initial byte {index}");
            }
            let start = required(record, "start_nibble")
                .as_u64()
                .expect("start_nibble must be a nonnegative integer");
            let end = required(record, "end_nibble")
                .as_i64()
                .expect("end_nibble must be an integer");
            assert!(matches!(return_code, -22 | -12) || return_code >= 0);
            if api == "osmo_bcd2str" {
                assert!(end >= 0, "{case} decode end must be nonnegative");
                assert!(
                    end <= (source.len() * 2) as i64,
                    "{case} decode source bounds"
                );
                if !after.is_empty() {
                    assert!(after.contains(&0), "{case} decode must be NUL terminated");
                }
                if return_code >= 0 {
                    assert_eq!(
                        return_code,
                        (end - start as i64).max(0),
                        "{case} decoded length"
                    );
                }
            } else {
                assert!(source.contains(&0), "{case} C string must be terminated");
                assert!(end >= -1, "{case} encode end must be -1 or nonnegative");
                if return_code >= 0 {
                    assert!(return_code <= dst_len as i64, "{case} encoded length");
                }
            }
        }
        assert_eq!(
            record.keys().map(String::as_str).collect::<BTreeSet<_>>(),
            expected_fields,
            "{case} fields"
        );
    }

    assert_eq!(observed_cases, expected_cases());
}
