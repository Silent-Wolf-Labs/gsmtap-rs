//! BCD digit and buffer records against `osmocore_rs::bcd` and its compat adapters.

use osmocore_rs::bcd::{BcdError, nibble_to_char};
use osmocore_rs::compat::bcd::{osmo_bcd2str, osmo_char2bcd, osmo_str2bcd};

use super::record::{self, Record};
use super::{Check, ModuleRun};

const EINVAL: i64 = -22;
const ENOMEM: i64 = -12;

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    let text = run.fixture("bcd/bcd_conversion_vectors.json")?;
    let records = record::array(&text)?;
    if let Some(first) = records.first() {
        run.revision(Record(first).str("libosmocore_commit")?);
    }
    for map in &records {
        let record = Record(map);
        let case = record.str("case").unwrap_or("<unnamed>");
        run.record(case, replay(&record).into());
    }
    Ok(())
}

fn replay(record: &Record<'_>) -> Result<Check, String> {
    let api = record.str("api")?;
    let source = record.hex("src_hex")?;
    let expected = record.int("return_code")?;
    let mut check = Check::default();
    match api {
        "osmo_bcd2char" | "osmo_char2bcd" => {
            let [byte] = source[..] else {
                return Err(format!("{api} records one source byte"));
            };
            if api == "osmo_char2bcd" {
                check.field("return_code", expected, i64::from(osmo_char2bcd(byte)));
            } else {
                match nibble_to_char(byte) {
                    Some(character) => check.field("return_code", expected, i64::from(character)),
                    None => check.rust_only("return_code", expected, "None"),
                }
            }
            return Ok(check);
        }
        "osmo_bcd2str" | "osmo_str2bcd" => {}
        other => return Err(format!("unsupported API `{other}`")),
    }

    let mut destination = record.hex("dst_before_hex")?;
    check.field("dst_len", record.usize("dst_len")?, destination.len());
    let start = record.usize("start_nibble")?;
    let end = record.int("end_nibble")?;
    let allow_hex = record.bool("allow_hex")?;
    let result = if api == "osmo_bcd2str" {
        let end = usize::try_from(end).map_err(|_| "decode end nibble is negative")?;
        osmo_bcd2str(&mut destination, &source, start, end, allow_hex)
    } else {
        // C's -1 end nibble means "consume the NUL-terminated string".
        osmo_str2bcd(
            &mut destination,
            &source,
            start,
            usize::try_from(end).ok(),
            allow_hex,
        )
    };
    match result {
        Ok(count) => check.field("return_code", expected, count as i64),
        Err(BcdError::InvalidDigit) => check.field("return_code", expected, EINVAL),
        Err(BcdError::NoSpace) => check.field("return_code", expected, ENOMEM),
        Err(error) => check.rust_only("return_code", expected, error),
    }
    check.bytes("dst_after_hex", &record.hex("dst_after_hex")?, &destination);
    Ok(check)
}
