use std::collections::BTreeSet;

use serde_json::{Map, Value};

const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
const CASES: &[&str] = &[
    "empty",
    "empty_zero_capacity",
    "whitespace_only",
    "zero_capacity_whitespace",
    "zero_capacity_digit",
    "lowercase",
    "uppercase",
    "mixed_case",
    "whitespace_between_digits",
    "whitespace_after_full_buffer",
    "exact_fit",
    "oversized_destination",
    "too_small_destination",
    "odd_single_digit",
    "odd_after_complete_byte",
    "invalid_first_character",
    "invalid_after_complete_byte",
    "invalid_after_high_nibble",
    "invalid_after_capacity",
    "embedded_nul",
    "high_bit_character",
];

fn required<'a>(record: &'a Map<String, Value>, field: &str) -> &'a Value {
    record
        .get(field)
        .unwrap_or_else(|| panic!("hexparse fixture is missing {field}"))
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

#[test]
fn hexparse_fixture_has_complete_c_observations() {
    let fixture = include_str!("vectors/hexparse/hexparse_buffer_vectors.json");
    let records: Vec<Map<String, Value>> =
        serde_json::from_str(fixture).expect("hexparse fixture must be a JSON array of objects");
    assert_eq!(records.len(), CASES.len());

    let expected_fields: BTreeSet<&str> = [
        "case",
        "libosmocore_commit",
        "src_hex",
        "dst_len",
        "dst_before_hex",
        "return_code",
        "dst_after_hex",
    ]
    .into_iter()
    .collect();
    let mut observed_cases = BTreeSet::new();

    for record in &records {
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
        assert_eq!(
            record.keys().map(String::as_str).collect::<BTreeSet<_>>(),
            expected_fields,
            "{case} fields"
        );

        let source = bytes(record, "src_hex");
        assert_eq!(source.last(), Some(&0), "{case} C source terminator");
        let dst_len = required(record, "dst_len")
            .as_u64()
            .and_then(|value| usize::try_from(value).ok())
            .expect("dst_len must fit usize");
        let before = bytes(record, "dst_before_hex");
        let after = bytes(record, "dst_after_hex");
        assert_eq!(before.len(), dst_len, "{case} initial destination length");
        assert_eq!(after.len(), dst_len, "{case} final destination length");
        for (index, byte) in before.iter().enumerate() {
            assert_eq!(*byte, 0xa5 ^ index as u8, "{case} initial byte {index}");
        }
        let return_code = required(record, "return_code")
            .as_i64()
            .expect("return_code must be an integer");
        assert!(
            return_code == -1 || (0..=dst_len as i64).contains(&return_code),
            "{case} return code is outside the buffer contract"
        );
    }

    assert_eq!(
        observed_cases,
        CASES.iter().map(ToString::to_string).collect()
    );
}
