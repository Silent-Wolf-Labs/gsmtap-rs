//! Packed/unpacked bit records against `osmocore_rs::bits`.

use osmocore_rs::bits::{pack_into, pack_into_offset, unpack_into, unpack_into_offset};

use super::record::{self, Record};
use super::{Check, ModuleRun};

const FIXTURES: &[&str] = &[
    "bits/bit_packing_vectors.json",
    "bits/bit_packing_ext_vectors.json",
];

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    for path in FIXTURES {
        let text = run.fixture(path)?;
        let records = record::array(&text)?;
        if let Some(first) = records.first() {
            run.revision(Record(first).str("libosmocore_commit")?);
        }
        for map in &records {
            let record = Record(map);
            let case = record.str("case").unwrap_or("<unnamed>");
            run.record(case, replay(&record).into());
        }
    }
    Ok(())
}

fn replay(record: &Record<'_>) -> Result<Check, String> {
    let api = record.str("api")?;
    let source = record.hex("src_hex")?;
    let mut destination = record.hex("dst_before_hex")?;
    let num_bits = record.usize("num_bits")?;
    let mut check = Check::default();
    check.field("dst_len", record.usize("dst_len")?, destination.len());
    let result = match api {
        "osmo_ubit2pbit" => pack_into(&mut destination, &source, num_bits),
        "osmo_pbit2ubit" => unpack_into(&mut destination, &source, num_bits),
        "osmo_ubit2pbit_ext" | "osmo_pbit2ubit_ext" => {
            // osmocore-rs reproduces the zero-bit result of a 32-bit C unsigned int.
            let unsigned_bits = record.uint("c_unsigned_bits")?;
            if unsigned_bits != 32 {
                return Err(format!(
                    "recorded with a {unsigned_bits}-bit C unsigned int"
                ));
            }
            let out_ofs = record.usize("out_ofs")?;
            let in_ofs = record.usize("in_ofs")?;
            // C treats any nonzero `int` lsb_mode, including negative values, as true.
            let lsb_mode = record.int("lsb_mode")? != 0;
            let convert = if api == "osmo_ubit2pbit_ext" {
                pack_into_offset
            } else {
                unpack_into_offset
            };
            convert(
                &mut destination,
                out_ofs,
                &source,
                in_ofs,
                num_bits,
                lsb_mode,
            )
        }
        other => return Err(format!("unsupported API `{other}`")),
    };
    let expected = record.int("return_code")?;
    match result {
        Ok(count) => check.field("return_code", expected, count as i64),
        Err(error) => check.rust_only("return_code", expected, error),
    }
    check.bytes("dst_after_hex", &record.hex("dst_after_hex")?, &destination);
    Ok(check)
}
