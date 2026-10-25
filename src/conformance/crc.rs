//! CRC-16 table/byte/buffer and generic bit-CRC records against osmocore-rs.
//!
//! The generic replay follows `reference/crc_vector.c`: compute, set, check,
//! then check again with each CRC bit flipped and with each bit made nonbinary.

use osmocore_rs::compat::crc::{
    OSMO_CRC16_CCITT_TABLE, OSMO_CRC16_TABLE, osmo_crc8gen_check_bits, osmo_crc8gen_compute_bits,
    osmo_crc8gen_set_bits, osmo_crc16, osmo_crc16_byte, osmo_crc16_ccitt, osmo_crc16_ccitt_byte,
    osmo_crc16gen_check_bits, osmo_crc16gen_compute_bits, osmo_crc16gen_set_bits,
    osmo_crc32gen_check_bits, osmo_crc32gen_compute_bits, osmo_crc32gen_set_bits,
    osmo_crc64gen_check_bits, osmo_crc64gen_compute_bits, osmo_crc64gen_set_bits,
};
use osmocore_rs::crc::{Crc8Code, Crc16Code, Crc32Code, Crc64Code, CrcError};

use super::record::{self, Record};
use super::{Check, ModuleRun};

const FIXTURES: &[&str] = &["crc/crc_byte_vectors.json", "crc/crc_generic_vectors.json"];

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    for path in FIXTURES {
        let text = run.fixture(path)?;
        for map in record::lines(&text).map_err(|error| format!("{path}: {error}"))? {
            let record = Record(&map);
            let kind = record.str("kind").unwrap_or("<unknown>");
            if kind == "metadata" {
                run.revision(record.str("revision")?);
                continue;
            }
            let case = format!("{kind}/{}", record.int("case").unwrap_or(-1));
            let outcome = match kind {
                "table" | "byte" | "buffer" => byte(&record, kind),
                "generic" => generic(&record),
                other => Err(format!("unsupported record kind `{other}`")),
            };
            run.record(case, outcome.into());
        }
    }
    Ok(())
}

fn byte(record: &Record<'_>, kind: &str) -> Result<Check, String> {
    let ccitt = match record.uint("flavor")? {
        0 => false,
        1 => true,
        other => return Err(format!("unknown CRC-16 flavor {other}")),
    };
    let mut check = Check::default();
    let expected = record.uint("result")?;
    match kind {
        "table" => {
            let table = if ccitt {
                &OSMO_CRC16_CCITT_TABLE
            } else {
                &OSMO_CRC16_TABLE
            };
            let index = record.usize("byte")?;
            let entry = table.get(index).ok_or("table index is out of range")?;
            check.field("result", expected, u64::from(*entry));
        }
        "byte" => {
            let init = narrow(record.uint("init")?)?;
            let data = narrow(record.uint("byte")?)?;
            let update = if ccitt {
                osmo_crc16_ccitt_byte
            } else {
                osmo_crc16_byte
            };
            check.field("result", expected, u64::from(update(init, data)));
        }
        _ => {
            let input = record.hex("before")?;
            let init: u16 = narrow(record.uint("init")?)?;
            let len = record.usize("len")?;
            let split = record.usize("split")?;
            if split > len {
                return Err("split is beyond the length".to_owned());
            }
            let compute = if ccitt { osmo_crc16_ccitt } else { osmo_crc16 };
            let mid = compute(init, &input, split);
            check.field("mid", Ok(record.uint("mid")?), mid.map(u64::from));
            if let Ok(mid) = mid {
                let tail = input.get(split..).unwrap_or_default();
                let continuation = compute(mid, tail, len - split).map(u64::from);
                check.field(
                    "continuation",
                    Ok(record.uint("continuation")?),
                    continuation,
                );
            }
            let result = compute(init, &input, len).map(u64::from);
            check.field("result", Ok(expected), result);
            check.bytes("after", &record.hex("after")?, &input);
        }
    }
    Ok(check)
}

/// Results of one generic replay, in the order the C harness records them.
struct Generic {
    result: Result<u64, CrcError>,
    after: Vec<u8>,
    status: Result<i32, CrcError>,
    flips: Vec<Result<i32, CrcError>>,
    nonbinary: Vec<Result<i32, CrcError>>,
}

macro_rules! replay_width {
    ($code:ident, $ty:ty, $compute:ident, $set:ident, $check:ident, $record:expr, $source:expr, $len:expr, $out:expr) => {{
        let record = $record;
        let code = $code {
            bits: record.i32("bits")?,
            poly: narrow::<$ty>(record.uint("poly")?)?,
            init: narrow::<$ty>(record.uint("init")?)?,
            remainder: narrow::<$ty>(record.uint("remainder")?)?,
        };
        let mut out: Vec<u8> = $out;
        let result = $compute(&code, $source, $len).map(|value| value as u64);
        let set = $set(&code, $source, $len, &mut out);
        let status = set.and_then(|()| $check(&code, $source, $len, &out));
        let mut flips = Vec::new();
        let mut nonbinary = Vec::new();
        for bit in 0..code.bits.max(0) as usize {
            let Some(original) = out.get(bit).copied() else {
                break;
            };
            out[bit] = original ^ 1;
            flips.push($check(&code, $source, $len, &out));
            out[bit] = original ^ 2;
            nonbinary.push($check(&code, $source, $len, &out));
            out[bit] = original;
        }
        Generic {
            result,
            after: out,
            status,
            flips,
            nonbinary,
        }
    }};
}

fn generic(record: &Record<'_>) -> Result<Check, String> {
    let input = record.hex("input_before")?;
    let len = record.i32("len")?;
    // The harness passes NULL for nonpositive lengths.
    let source: &[u8] = if len <= 0 { &[] } else { &input };
    let before = record.hex("before")?;
    let mut check = Check::default();
    check.field("capacity", record.usize("capacity")?, before.len());
    let replay = match record.uint("width")? {
        8 => replay_width!(
            Crc8Code,
            u8,
            osmo_crc8gen_compute_bits,
            osmo_crc8gen_set_bits,
            osmo_crc8gen_check_bits,
            record,
            source,
            len,
            before
        ),
        16 => replay_width!(
            Crc16Code,
            u16,
            osmo_crc16gen_compute_bits,
            osmo_crc16gen_set_bits,
            osmo_crc16gen_check_bits,
            record,
            source,
            len,
            before
        ),
        32 => replay_width!(
            Crc32Code,
            u32,
            osmo_crc32gen_compute_bits,
            osmo_crc32gen_set_bits,
            osmo_crc32gen_check_bits,
            record,
            source,
            len,
            before
        ),
        64 => replay_width!(
            Crc64Code,
            u64,
            osmo_crc64gen_compute_bits,
            osmo_crc64gen_set_bits,
            osmo_crc64gen_check_bits,
            record,
            source,
            len,
            before
        ),
        other => return Err(format!("unsupported CRC width {other}")),
    };
    check.field("result", Ok(record.uint("result")?), replay.result);
    check.bytes("after", &record.hex("after")?, &replay.after);
    check.field("match", Ok(record.i32("match")?), replay.status);
    let as_status = |bytes: Vec<u8>| bytes.into_iter().map(|byte| Ok(i32::from(byte))).collect();
    check.field::<Vec<Result<i32, CrcError>>>(
        "flips",
        as_status(record.hex("flips")?),
        replay.flips,
    );
    check.field::<Vec<Result<i32, CrcError>>>(
        "nonbinary",
        as_status(record.hex("nonbinary")?),
        replay.nonbinary,
    );
    check.bytes("input_after", &record.hex("input_after")?, &input);
    Ok(check)
}

fn narrow<T: TryFrom<u64>>(value: u64) -> Result<T, String> {
    T::try_from(value).map_err(|_| format!("value {value} does not fit its C type"))
}
