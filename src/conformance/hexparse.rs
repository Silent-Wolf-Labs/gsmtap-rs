//! `osmo_hexparse` records against `osmocore_rs::hexparse::parse_into`.

use osmocore_rs::hexparse::parse_into;

use super::record::{self, Record};
use super::{Check, ModuleRun};

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    let text = run.fixture("hexparse/hexparse_buffer_vectors.json")?;
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
    let mut destination = record.hex("dst_before_hex")?;
    let mut check = Check::default();
    check.field("dst_len", record.usize("dst_len")?, destination.len());
    // C returns -1 for every parse failure.
    let return_code = parse_into(&source, &mut destination).map_or(-1, |length| length as i64);
    check.field("return_code", record.int("return_code")?, return_code);
    check.bytes("dst_after_hex", &record.hex("dst_after_hex")?, &destination);
    Ok(check)
}
