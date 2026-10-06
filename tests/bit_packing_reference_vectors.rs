use std::collections::{BTreeMap, BTreeSet};

use serde_json::{Map, Value};

const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
const CASE_COUNT: usize = 9_016;
const LENGTHS: &[usize] = &[7, 8, 9, 15, 16, 17, 31, 32, 33];
const FIXTURE: &str = include_str!("vectors/bits/bit_packing_vectors.json");

struct Case {
    name: String,
    packing: bool,
    bits: usize,
    source: Vec<u8>,
    dst_len: usize,
}

fn packed_size(bits: usize) -> usize {
    bits / 8 + usize::from(bits % 8 != 0)
}

// This freezes input construction and case order, not converted output. The
// destination observations remain C-produced; Rust API comparison lives in
// the sibling crate. Change this manifest only with fixture-contract review.
fn expected_cases() -> Vec<Case> {
    let mut cases = Vec::new();
    for byte in 0..=255u8 {
        let unpacked: Vec<u8> = (0..8).map(|i| (byte >> (7 - i)) & 1).collect();
        for bits in 1..=8 {
            for packing in [false, true] {
                for extra in [false, true] {
                    cases.push(Case {
                        name: format!(
                            "{}_byte_{byte:02x}_bits_{bits}_{}",
                            if packing { "pack" } else { "unpack" },
                            if extra { "oversized" } else { "exact" }
                        ),
                        packing,
                        bits,
                        source: if packing {
                            unpacked.clone()
                        } else {
                            vec![byte]
                        },
                        dst_len: if packing { packed_size(bits) } else { bits }
                            + if extra { 3 } else { 0 },
                    });
                }
            }
        }
    }
    for &bits in LENGTHS {
        for pattern in 0..3 + bits {
            let name = match pattern {
                0 => "zero".to_owned(),
                1 => "one".to_owned(),
                2 => "alternating".to_owned(),
                _ => format!("single_{:02}", pattern - 3),
            };
            let unpacked: Vec<u8> = (0..bits)
                .map(|i| {
                    u8::from(
                        pattern == 1
                            || (pattern == 2 && i % 2 == 0)
                            || (pattern >= 3 && i == pattern - 3),
                    )
                })
                .collect();
            let mut packed = vec![0; packed_size(bits)];
            // Input construction for a known pattern; never used as expected
            // destination output of a conversion.
            for (i, &bit) in unpacked.iter().enumerate() {
                packed[i / 8] |= bit << (7 - i % 8);
            }
            for packing in [false, true] {
                for extra in [false, true] {
                    cases.push(Case {
                        name: format!(
                            "{}_boundary_{bits}_{name}_{}",
                            if packing { "pack" } else { "unpack" },
                            if extra { "oversized" } else { "exact" }
                        ),
                        packing,
                        bits,
                        source: if packing {
                            unpacked.clone()
                        } else {
                            packed.clone()
                        },
                        dst_len: if packing { packed_size(bits) } else { bits }
                            + if extra { 3 } else { 0 },
                    });
                }
            }
        }
        for packing in [false, true] {
            for tail in 0..=1 {
                let mut source = vec![
                    if packing { 1 } else { 0xff };
                    if packing { bits } else { packed_size(bits) }
                ];
                source.extend([if packing { tail } else { tail * 0xff }; 3]);
                cases.push(Case {
                    name: format!(
                        "{}_extra_bits_{bits}_tail_{tail}",
                        if packing { "pack" } else { "unpack" }
                    ),
                    packing,
                    bits,
                    source,
                    dst_len: if packing { packed_size(bits) } else { bits } + 3,
                });
            }
        }
    }
    for packing in [false, true] {
        for high in 0..=1 {
            for extra in [false, true] {
                cases.push(Case {
                    name: format!(
                        "{}_zero_bit_{high}_{}",
                        if packing { "pack" } else { "unpack" },
                        if extra { "oversized" } else { "backed" }
                    ),
                    packing,
                    bits: 0,
                    source: vec![if packing { high } else { high * 0x80 }],
                    dst_len: if extra { 4 } else { 1 },
                });
            }
        }
    }
    assert_eq!(cases.len(), CASE_COUNT, "frozen case count");
    cases
}

fn required<'a>(record: &'a Map<String, Value>, field: &str) -> &'a Value {
    record
        .get(field)
        .unwrap_or_else(|| panic!("bit-packing fixture is missing {field}"))
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
        .map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap())
        .collect()
}

fn unsigned(record: &Map<String, Value>, field: &str) -> usize {
    required(record, field)
        .as_u64()
        .and_then(|value| usize::try_from(value).ok())
        .unwrap_or_else(|| panic!("{field} must be a nonnegative integer fitting usize"))
}

fn validate_record(record: &Map<String, Value>, expected: &Case) -> Vec<u8> {
    let case = &expected.name;
    let fields: BTreeSet<&str> = [
        "case",
        "api",
        "libosmocore_commit",
        "num_bits",
        "src_hex",
        "dst_len",
        "dst_before_hex",
        "return_code",
        "dst_after_hex",
    ]
    .into_iter()
    .collect();
    assert_eq!(
        record.keys().map(String::as_str).collect::<BTreeSet<_>>(),
        fields,
        "{case} fields"
    );
    assert_eq!(
        required(record, "case").as_str(),
        Some(case.as_str()),
        "case order and name"
    );
    let api = if expected.packing {
        "osmo_ubit2pbit"
    } else {
        "osmo_pbit2ubit"
    };
    assert_eq!(required(record, "api").as_str(), Some(api), "{case} API");
    assert_eq!(
        required(record, "libosmocore_commit").as_str(),
        Some(LIBOSMOCORE_COMMIT),
        "{case} provenance"
    );
    let bits = unsigned(record, "num_bits");
    assert!(u32::try_from(bits).is_ok(), "{case} C unsigned bit count");
    assert_eq!(bits, expected.bits, "{case} bit count");
    let source = bytes(record, "src_hex");
    assert_eq!(source, expected.source, "{case} complete source");
    let source_used = if expected.packing {
        bits
    } else {
        packed_size(bits).max(1)
    };
    assert!(source.len() >= source_used, "{case} safe C source backing");
    if expected.packing {
        assert!(
            source.iter().all(|&byte| byte <= 1),
            "{case} unpacked input domain"
        );
    }
    let dst_len = unsigned(record, "dst_len");
    assert_eq!(
        dst_len, expected.dst_len,
        "{case} destination backing length"
    );
    let before = bytes(record, "dst_before_hex");
    let after = bytes(record, "dst_after_hex");
    assert_eq!(before.len(), dst_len, "{case} complete initial destination");
    assert_eq!(after.len(), dst_len, "{case} complete final destination");
    for (i, &byte) in before.iter().enumerate() {
        assert_eq!(byte, 0xa5 ^ i as u8, "{case} sentinel at {i}");
    }
    let written = if expected.packing {
        packed_size(bits)
    } else {
        bits.max(1)
    };
    assert!(
        dst_len >= written && dst_len > 0,
        "{case} safe C destination backing"
    );
    let rc = required(record, "return_code")
        .as_i64()
        .expect("return_code must be an integer");
    assert!(i32::try_from(rc).is_ok(), "{case} C int return value");
    assert_eq!(rc, written as i64, "{case} observed return count");
    assert_eq!(
        &after[written..],
        &before[written..],
        "{case} untouched suffix"
    );
    if expected.packing && bits % 8 != 0 {
        let unused_mask = (1u8 << (8 - bits % 8)) - 1;
        assert_eq!(
            after[written - 1] & unused_mask,
            0,
            "{case} cleared unused low bits"
        );
    } else if !expected.packing {
        assert!(
            after[..written].iter().all(|&byte| byte <= 1),
            "{case} unpacked output domain"
        );
        if bits == 0 {
            assert_eq!(after[0], source[0] >> 7, "{case} actual zero-bit C write");
        }
    }
    after[..written].to_vec()
}

#[test]
fn bit_packing_fixture_has_complete_c_observations() {
    let records: Vec<Map<String, Value>> =
        serde_json::from_str(FIXTURE).expect("bit-packing fixture must be a JSON array of objects");
    let cases = expected_cases();
    assert_eq!(records.len(), CASE_COUNT);
    let mut names = BTreeSet::new();
    let mut observations = BTreeMap::new();
    for (record, expected) in records.iter().zip(&cases) {
        assert!(names.insert(&expected.name), "duplicate case name");
        let output = validate_record(record, expected);
        let used = if expected.packing {
            expected.bits
        } else {
            packed_size(expected.bits).max(1)
        };
        let key = (
            expected.packing,
            expected.bits,
            expected.source[..used].to_vec(),
        );
        if let Some(previous) = observations.insert(key, output.clone()) {
            assert_eq!(
                output, previous,
                "{} capacity/source suffix independence",
                expected.name
            );
        }
    }
}

#[test]
fn validator_rejects_corrupt_observations() {
    let records: Vec<Map<String, Value>> = serde_json::from_str(FIXTURE).unwrap();
    let cases = expected_cases();
    let first = &cases[0];
    for (field, value) in [
        ("case", Value::from("unexpected")),
        ("api", Value::from("osmo_ubit2pbit_ext")),
        ("libosmocore_commit", Value::from("unverified")),
        ("num_bits", Value::from(1.0)),
        ("src_hex", Value::from("FF")),
        ("dst_len", Value::from(0)),
        ("dst_before_hex", Value::from("a4")),
        ("return_code", Value::from(0)),
        ("dst_after_hex", Value::from("")),
        ("unexpected", Value::Null),
    ] {
        let mut record = records[0].clone();
        record.insert(field.to_owned(), value);
        assert!(
            std::panic::catch_unwind(|| validate_record(&record, first)).is_err(),
            "accepted corrupt {field}"
        );
    }
    // A normalized zero-bit result must never replace the actual C anomaly.
    let index = cases
        .iter()
        .position(|case| case.name == "unpack_zero_bit_1_backed")
        .unwrap();
    let mut record = records[index].clone();
    record.insert("return_code".to_owned(), Value::from(0));
    record.insert("dst_after_hex".to_owned(), Value::from("a5"));
    assert!(std::panic::catch_unwind(|| validate_record(&record, &cases[index])).is_err());
}
