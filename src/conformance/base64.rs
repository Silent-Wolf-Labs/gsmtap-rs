//! Base64 buffer records against the C-shaped osmocore-rs adapters.

use osmocore_rs::compat::base64::{osmo_base64_decode, osmo_base64_encode};

use super::record::{self, Record};
use super::{Check, ModuleRun};

/// Initial destination fill and `olen` set by `reference/base64_buffer_vector.c`.
const DESTINATION_FILL: u8 = 0xa5;
const OLEN_SENTINEL: usize = 0xdeca_fbad;

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    let text = run.fixture("base64/base64_buffer_vectors.json")?;
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
    let source = record.hex("src_hex")?;
    let mut destination = vec![DESTINATION_FILL; record.usize("dst_len")?];
    let mut olen = OLEN_SENTINEL;
    let return_code = match (record.str("operation")?, record.bool("dst_present")?) {
        ("encode", true) => osmo_base64_encode(&mut destination, &mut olen, &source),
        ("decode", true) => osmo_base64_decode(Some(&mut destination), &mut olen, &source),
        ("decode", false) => osmo_base64_decode(None, &mut olen, &source),
        (operation, present) => {
            return Err(format!(
                "unsupported operation `{operation}` with destination present: {present}"
            ));
        }
    };
    let mut check = Check::default();
    check.field(
        "return_code",
        record.int("return_code")?,
        i64::from(return_code),
    );
    check.field("olen", record.uint("olen")?, olen as u64);
    check.bytes("dst_after_hex", &record.hex("dst_after_hex")?, &destination);
    Ok(check)
}
