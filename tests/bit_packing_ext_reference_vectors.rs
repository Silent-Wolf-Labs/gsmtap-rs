use serde_json::{Map, Value};
use std::collections::{BTreeMap, BTreeSet};

const COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
const COUNT: usize = 63_384;
const OFFSETS: &[usize] = &[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16];
const COUNTS: &[usize] = &[1, 7, 8, 9, 15, 16, 17, 31, 32, 33];
const MODES: &[i32] = &[0, 1, 2, -1];
const FIXTURE: &str = include_str!("vectors/bits/bit_packing_ext_vectors.json");

struct Case {
    name: String,
    pack: bool,
    out: usize,
    input: usize,
    n: usize,
    mode: i32,
    src: Vec<u8>,
    before: Vec<u8>,
}
fn packed(n: usize) -> usize {
    n / 8 + usize::from(n % 8 != 0)
}
fn case(
    name: String,
    pack: bool,
    out: usize,
    input: usize,
    n: usize,
    mode: i32,
    src: Vec<u8>,
    dst_len: usize,
    init: usize,
) -> Case {
    Case {
        name,
        pack,
        out,
        input,
        n,
        mode,
        src,
        before: (0..dst_len)
            .map(|i| match init {
                0 => 0,
                1 => 255,
                _ => 0xa5 ^ i as u8,
            })
            .collect(),
    }
}
// Frozen input construction only. No converted output is generated here.
fn patterned(
    name: String,
    pack: bool,
    out: usize,
    input: usize,
    n: usize,
    mode: i32,
    pattern: usize,
    extra: usize,
    init: usize,
    tail: usize,
) -> Case {
    let used = if pack { input + n } else { packed(input + n) };
    let mut src = vec![if tail == 0 { 0 } else { 255 }; used + 3];
    for i in 0..n {
        let bit =
            pattern == 1 || (pattern == 2 && i % 2 == 0) || (pattern >= 3 && i == pattern - 3);
        let pos = input + i;
        if pack {
            src[pos] = u8::from(bit);
        } else {
            let bn = if mode != 0 { pos % 8 } else { 7 - pos % 8 };
            if bit {
                src[pos / 8] |= 1 << bn;
            } else {
                src[pos / 8] &= !(1 << bn);
            }
        }
    }
    case(
        name,
        pack,
        out,
        input,
        n,
        mode,
        src,
        (if pack { packed(out + n) } else { out + n }) + extra * 3,
        init,
    )
}
fn expected_cases() -> Vec<Case> {
    let mut cases = Vec::with_capacity(COUNT);
    for byte in 0..=255u8 {
        for ofs in 0..8 {
            for n in 1..=8 - ofs {
                for p in 0..=1 {
                    for mode in 0..=1 {
                        let pack = p == 1;
                        let out = if pack {
                            ofs
                        } else {
                            (usize::from(byte) + n) % 8
                        };
                        let input = if pack { 3 + ofs } else { ofs };
                        let mut src = vec![0xa5; if pack { 11 } else { 1 }];
                        if pack {
                            for i in 0..8 {
                                src[3 + i] = (byte >> if mode != 0 { i } else { 7 - i }) & 1;
                            }
                        } else {
                            src[0] = byte;
                        }
                        let seed = usize::from(byte) + ofs + n + p + mode as usize;
                        cases.push(case(
                            format!("exhaustive_b{byte:02x}_s{ofs}_n{n}_p{p}_m{mode}"),
                            pack,
                            out,
                            input,
                            n,
                            mode,
                            src,
                            (if pack { packed(out + n) } else { out + n }) + seed % 2 * 3,
                            seed % 3,
                        ));
                    }
                }
            }
        }
    }
    for (oi, &out) in OFFSETS.iter().enumerate() {
        for pairing in 0..2 {
            let input = OFFSETS[(oi + pairing) % 12];
            for (ni, &n) in COUNTS.iter().enumerate() {
                for pat in 0..3 + n {
                    for p in 0..=1 {
                        for mode in 0..=1 {
                            let seed = oi + pairing + ni + pat + p + mode as usize;
                            cases.push(patterned(
                                format!("boundary_o{out}_i{input}_n{n}_t{pat}_p{p}_m{mode}"),
                                p == 1,
                                out,
                                input,
                                n,
                                mode,
                                pat,
                                seed % 2,
                                seed % 3,
                                0,
                            ));
                        }
                    }
                }
            }
        }
    }
    for (oi, &out) in OFFSETS.iter().enumerate() {
        for (ii, &input) in OFFSETS.iter().enumerate() {
            for (ni, &n) in COUNTS.iter().enumerate() {
                for p in 0..=1 {
                    for mode in 0..=1 {
                        let seed = oi + ii + ni + p + mode as usize;
                        cases.push(patterned(
                            format!("offsets_o{out}_i{input}_n{n}_p{p}_m{mode}"),
                            p == 1,
                            out,
                            input,
                            n,
                            mode,
                            2,
                            seed % 2,
                            seed % 3,
                            0,
                        ));
                    }
                }
            }
        }
    }
    for value in [2, 0x80, 0xff] {
        for &out in OFFSETS {
            for &mode in MODES {
                for n in [1, 9, 17] {
                    for init in 0..3 {
                        let mut src = vec![0x5a; n + 6];
                        for i in 0..n {
                            src[3 + i] = if i % 2 == 0 { value } else { 0 };
                        }
                        cases.push(case(
                            format!("truthy_v{value:02x}_o{out}_n{n}_m{mode}_d{init}"),
                            true,
                            out,
                            3,
                            n,
                            mode,
                            src,
                            packed(out + n) + 3,
                            init,
                        ));
                    }
                }
            }
        }
    }
    for p in 0..=1 {
        for &mode in MODES {
            for &out in OFFSETS {
                for tail in 0..2 {
                    cases.push(patterned(
                        format!("neighbors_o{out}_p{p}_m{mode}_t{tail}"),
                        p == 1,
                        out,
                        9,
                        17,
                        mode,
                        2,
                        1,
                        2,
                        tail,
                    ));
                }
            }
        }
    }
    for p in 0..=1 {
        for (mi, &mode) in MODES.iter().enumerate() {
            for (oi, out) in [0, 1, 7, 8, 9, 16, 65].into_iter().enumerate() {
                for (ii, input) in [0, 9, 65].into_iter().enumerate() {
                    cases.push(case(
                        format!("zero_o{out}_i{input}_p{p}_m{mode}"),
                        p == 1,
                        out,
                        input,
                        0,
                        mode,
                        vec![255],
                        1,
                        (oi + ii + mi + p) % 3,
                    ));
                }
            }
        }
    }
    assert_eq!(cases.len(), COUNT, "frozen case count");
    cases
}
fn unsigned(record: &Map<String, Value>, key: &str) -> usize {
    record[key]
        .as_u64()
        .and_then(|v| usize::try_from(v).ok())
        .unwrap_or_else(|| panic!("{key} integer"))
}
fn signed(record: &Map<String, Value>, key: &str) -> i32 {
    record[key]
        .as_i64()
        .and_then(|v| i32::try_from(v).ok())
        .unwrap_or_else(|| panic!("{key} C int"))
}
fn bytes(record: &Map<String, Value>, key: &str) -> Vec<u8> {
    let hex = record[key].as_str().expect("hex string");
    assert_eq!(hex.len() % 2, 0, "complete bytes");
    assert!(
        hex.bytes()
            .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b)),
        "lowercase hex"
    );
    (0..hex.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap())
        .collect()
}
fn validate(record: &Map<String, Value>, c: &Case) -> Vec<u8> {
    let fields: BTreeSet<_> = [
        "case",
        "api",
        "libosmocore_commit",
        "c_unsigned_bits",
        "out_ofs",
        "in_ofs",
        "num_bits",
        "lsb_mode",
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
        "schema"
    );
    assert_eq!(record["case"].as_str(), Some(c.name.as_str()), "name/order");
    assert_eq!(
        record["api"].as_str(),
        Some(if c.pack {
            "osmo_ubit2pbit_ext"
        } else {
            "osmo_pbit2ubit_ext"
        })
    );
    assert_eq!(record["libosmocore_commit"].as_str(), Some(COMMIT));
    assert_eq!(unsigned(record, "c_unsigned_bits"), 32, "supported C ABI");
    for (key, expected) in [("out_ofs", c.out), ("in_ofs", c.input), ("num_bits", c.n)] {
        let value = unsigned(record, key);
        assert!(u32::try_from(value).is_ok(), "C unsigned range");
        assert_eq!(value, expected, "{key}");
    }
    assert_eq!(signed(record, "lsb_mode"), c.mode);
    let src = bytes(record, "src_hex");
    assert_eq!(src, c.src, "full source");
    let len = unsigned(record, "dst_len");
    assert_eq!(len, c.before.len());
    let before = bytes(record, "dst_before_hex");
    let after = bytes(record, "dst_after_hex");
    assert_eq!(before, c.before, "initialized destination");
    assert_eq!(after.len(), len, "complete final destination");
    assert!(
        !src.is_empty() && len > 0,
        "real backing even for zero bits"
    );
    let rc = signed(record, "return_code");
    let out_end = c.out.checked_add(c.n).unwrap();
    let input_end = c.input.checked_add(c.n).unwrap();
    assert!(u32::try_from(out_end).is_ok() && u32::try_from(input_end).is_ok());
    let expected_rc = if c.pack {
        if c.n == 0 && c.out == 0 {
            536_870_912
        } else {
            packed(out_end)
        }
    } else {
        out_end
    };
    assert!(i32::try_from(expected_rc).is_ok(), "representable C return");
    assert_eq!(i64::from(rc), expected_rc as i64, "observed return");
    if c.n == 0 {
        assert_eq!(after, before, "zero bits perform no writes");
        return after;
    }
    assert!(
        src.len() >= if c.pack { input_end } else { packed(input_end) },
        "safe source backing"
    );
    assert!(
        len >= if c.pack { packed(out_end) } else { out_end },
        "safe destination backing"
    );
    if c.pack {
        for (i, (&a, &b)) in after.iter().zip(&before).enumerate() {
            let mut touched = 0u8;
            for position in i * 8..i * 8 + 8 {
                if (c.out..out_end).contains(&position) {
                    touched |= 1
                        << if c.mode != 0 {
                            position % 8
                        } else {
                            7 - position % 8
                        };
                }
            }
            assert_eq!(a & !touched, b & !touched, "untouched packed bits at {i}");
        }
    } else {
        assert_eq!(after[..c.out], before[..c.out], "untouched prefix");
        assert_eq!(after[out_end..], before[out_end..], "untouched suffix");
        assert!(
            after[c.out..out_end].iter().all(|&v| v <= 1),
            "binary output"
        );
    }
    after
}
#[test]
fn extended_fixture_has_complete_c_observations() {
    let records: Vec<Map<String, Value>> = serde_json::from_str(FIXTURE).unwrap();
    let cases = expected_cases();
    assert_eq!(records.len(), COUNT);
    let mut names = BTreeSet::new();
    let mut equivalents = BTreeMap::new();
    for (record, c) in records.iter().zip(&cases) {
        assert!(names.insert(&c.name), "unique case");
        let after = validate(record, c);
        // Compare C observations for deliberately identical requests; this
        // supplies no independent implementation of expected output.
        let key = if c.name.starts_with("neighbors_") {
            Some(format!(
                "neighbors_o{}_p{}_lsb{}",
                c.out,
                c.pack,
                c.mode != 0
            ))
        } else if c.name.starts_with("truthy_") && c.mode != 0 {
            Some(format!("truthy_o{}_n{}_d{:?}", c.out, c.n, c.before))
        } else {
            None
        };
        if let Some(key) = key {
            if let Some(previous) = equivalents.insert(key, after.clone()) {
                assert_eq!(
                    previous, after,
                    "source neighbors/truthiness/nonzero mode independence"
                );
            }
        }
    }
}
#[test]
fn validator_rejects_corrupt_records() {
    let records: Vec<Map<String, Value>> = serde_json::from_str(FIXTURE).unwrap();
    let cases = expected_cases();
    for (field, value) in [
        ("case", Value::from("bad")),
        ("api", Value::from("osmo_pbit2ubit")),
        ("libosmocore_commit", Value::from("unverified")),
        ("c_unsigned_bits", Value::from(64)),
        ("out_ofs", Value::from(-1)),
        ("in_ofs", Value::from(1.0)),
        ("num_bits", Value::from(4294967296u64)),
        ("lsb_mode", Value::from(0.0)),
        ("src_hex", Value::from("FF")),
        ("dst_len", Value::from(0)),
        ("dst_before_hex", Value::from("01")),
        ("return_code", Value::from(2147483648i64)),
        ("dst_after_hex", Value::from("0")),
        ("unexpected", Value::Null),
    ] {
        let mut record = records[0].clone();
        record.insert(field.to_owned(), value);
        assert!(
            std::panic::catch_unwind(|| validate(&record, &cases[0])).is_err(),
            "accepted {field}"
        );
    }
    // Every zero-count return is checked, including both APIs and offsets
    // beyond backing. Changing any one must fail even though no bytes change.
    for (index, c) in cases.iter().enumerate().filter(|(_, c)| c.n == 0) {
        let mut record = records[index].clone();
        let changed = signed(&record, "return_code") ^ 1;
        record.insert("return_code".into(), Value::from(changed));
        assert!(std::panic::catch_unwind(|| validate(&record, c)).is_err());
    }
    let index = cases
        .iter()
        .position(|c| c.pack && c.n == 0 && c.out == 0)
        .unwrap();
    let mut record = records[index].clone();
    record.insert("return_code".into(), Value::from(0));
    assert!(
        std::panic::catch_unwind(|| validate(&record, &cases[index])).is_err(),
        "normalized zero return"
    );
    // Corrupt every untouched storage bit in representative partial-byte,
    // prefixed, oversized and zero-count destinations, in both bit orders.
    for pack in [false, true] {
        for mode in [0, 1] {
            for zero in [false, true] {
                let index = cases
                    .iter()
                    .position(|c| {
                        c.pack == pack
                            && c.mode == mode
                            && if zero {
                                c.n == 0
                            } else {
                                c.out == 1
                                    && c.n == 9
                                    && c.before.len()
                                        > if pack {
                                            packed(c.out + c.n)
                                        } else {
                                            c.out + c.n
                                        }
                            }
                    })
                    .unwrap();
                let c = &cases[index];
                for byte in 0..c.before.len() {
                    for bn in 0..8 {
                        let position = if pack {
                            byte * 8 + if mode != 0 { bn } else { 7 - bn }
                        } else {
                            byte
                        };
                        if c.n > 0 && (c.out..c.out + c.n).contains(&position) {
                            continue;
                        }
                        let mut record = records[index].clone();
                        let mut after = bytes(&record, "dst_after_hex");
                        after[byte] ^= 1 << bn;
                        record.insert(
                            "dst_after_hex".into(),
                            Value::from(
                                after.iter().map(|v| format!("{v:02x}")).collect::<String>(),
                            ),
                        );
                        assert!(
                            std::panic::catch_unwind(|| validate(&record, c)).is_err(),
                            "changed untouched bit"
                        );
                    }
                }
            }
        }
    }
    let mut record = records[0].clone();
    record.remove("api");
    assert!(std::panic::catch_unwind(|| validate(&record, &cases[0])).is_err());
}
