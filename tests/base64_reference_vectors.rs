use std::collections::BTreeSet;

use serde_json::{Map, Value};

const FIXTURE: &str = include_str!("vectors/base64/base64_buffer_vectors.json");
const FIXTURE_FINGERPRINT: u64 = 0xc0131a2b09b38ab4;
const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
// The harness fills every destination byte and the caller's output length with
// these values before each call.
const DST_FILL: u8 = 0xa5;
const OLEN_SENTINEL: u64 = 0xdecafbad;
const EINVAL: i64 = -22;
const ENOBUFS: i64 = -105;
const CASES: &[&str] = &[
    "encode_empty",
    "encode_one_byte_padding",
    "encode_two_byte_padding",
    "encode_three_bytes",
    "encode_multi_block",
    "encode_upstream_test_vector",
    "encode_size_probe",
    "encode_too_small",
    "decode_padded",
    "decode_unpadded",
    "decode_trailing_spaces",
    "decode_newline",
    "decode_crlf",
    "decode_space_inside_quartet",
    "decode_invalid_character",
    "decode_misplaced_padding",
    "decode_excessive_padding",
    "decode_high_bit_character",
    "decode_null_size_probe",
    "decode_zero_size_probe",
    "decode_too_small",
    "decode_empty",
    "decode_spaces_only",
    "decode_newlines_only",
];

fn fingerprint(text: &str) -> u64 {
    text.bytes().fold(0xcbf29ce484222325, |hash, byte| {
        (hash ^ u64::from(byte)).wrapping_mul(0x100000001b3)
    })
}

fn required<'a>(record: &'a Map<String, Value>, field: &str) -> &'a Value {
    record
        .get(field)
        .unwrap_or_else(|| panic!("base64 fixture is missing {field}"))
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
fn base64_fixture_is_unchanged() {
    assert_eq!(
        fingerprint(FIXTURE),
        FIXTURE_FINGERPRINT,
        "direct C observations changed"
    );
}

#[test]
fn base64_fixture_has_complete_c_observations() {
    let records: Vec<Map<String, Value>> =
        serde_json::from_str(FIXTURE).expect("base64 fixture must be a JSON array of objects");
    let observed: Vec<&str> = records
        .iter()
        .map(|record| {
            required(record, "case")
                .as_str()
                .expect("case must be a string")
        })
        .collect();
    assert_eq!(observed, CASES, "case names and order");

    let expected_fields: BTreeSet<&str> = [
        "case",
        "operation",
        "libosmocore_commit",
        "src_hex",
        "dst_present",
        "dst_len",
        "return_code",
        "olen",
        "dst_after_hex",
    ]
    .into_iter()
    .collect();

    for (record, case) in records.iter().zip(CASES) {
        assert_eq!(
            record.keys().map(String::as_str).collect::<BTreeSet<_>>(),
            expected_fields,
            "{case} fields"
        );
        assert_eq!(
            required(record, "libosmocore_commit").as_str(),
            Some(LIBOSMOCORE_COMMIT),
            "{case} has unexpected C provenance"
        );
        let operation = required(record, "operation")
            .as_str()
            .expect("operation must be a string");
        assert!(
            case.starts_with(&format!("{operation}_")),
            "{case} records operation {operation}"
        );
        bytes(record, "src_hex");

        let present = required(record, "dst_present")
            .as_bool()
            .expect("dst_present must be a boolean");
        let dst_len = required(record, "dst_len")
            .as_u64()
            .and_then(|value| usize::try_from(value).ok())
            .expect("dst_len must fit usize");
        let after = bytes(record, "dst_after_hex");
        if present {
            assert_eq!(after.len(), dst_len, "{case} final destination length");
        } else {
            assert_eq!(operation, "decode", "{case} only decode accepts NULL");
            assert_eq!(dst_len, 0, "{case} NULL destination length");
            assert!(after.is_empty(), "{case} NULL destination contents");
        }

        let return_code = required(record, "return_code")
            .as_i64()
            .expect("return_code must be an integer");
        let olen = required(record, "olen")
            .as_u64()
            .expect("olen must be an unsigned integer");
        match return_code {
            0 => {
                // Bytes past the reported length, plus encode's terminator,
                // stay as initialized.
                if olen != OLEN_SENTINEL {
                    let olen = usize::try_from(olen).unwrap();
                    assert!(olen <= dst_len, "{case} output exceeds destination");
                    let written = if operation == "encode" {
                        (olen + 1).min(dst_len)
                    } else {
                        olen
                    };
                    assert!(
                        after[written..].iter().all(|&byte| byte == DST_FILL),
                        "{case} wrote past its reported length"
                    );
                }
            }
            ENOBUFS => {
                assert!(
                    olen > dst_len as u64,
                    "{case} must report a size larger than its destination"
                );
                assert!(
                    after.iter().all(|&byte| byte == DST_FILL),
                    "{case} changed its destination"
                );
            }
            EINVAL => {
                assert_eq!(operation, "decode", "{case} only decode rejects input");
                assert_eq!(olen, OLEN_SENTINEL, "{case} changed olen on error");
                assert!(
                    after.iter().all(|&byte| byte == DST_FILL),
                    "{case} changed its destination"
                );
            }
            other => panic!("{case} has unexpected return code {other}"),
        }
    }
}
