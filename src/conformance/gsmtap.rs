//! `gsmtap_makemsg[_ex]` records against `osmocore_rs::gsmtap::GsmtapMessage`.

use osmocore_rs::gsmtap::GsmtapMessage;

use super::record::{self, Record};
use super::{Check, ModuleRun};

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    let mut paths: Vec<String> = std::fs::read_dir(run.vectors)
        .map_err(|error| format!("cannot list vectors: {error}"))?
        .filter_map(|entry| entry.ok()?.file_name().into_string().ok())
        .filter(|name| name.starts_with("gsmtap_") && name.ends_with(".json"))
        .collect();
    if paths.is_empty() {
        return Err("no gsmtap_*.json fixtures".to_owned());
    }
    paths.sort();
    for path in paths {
        let text = run.fixture(&path)?;
        let map = serde_json::from_str(&text)
            .map_err(|error| format!("{path}: invalid JSON: {error}"))
            .and_then(record::object)?;
        let record = Record(&map);
        run.revision(record.str("libosmocore_commit")?);
        let case = record.str("case").unwrap_or("<unnamed>");
        run.record(case, replay(&record).into());
    }
    Ok(())
}

fn replay(record: &Record<'_>) -> Result<Check, String> {
    if record.int("return_code")? != 0 || !record.bool("message_created")? {
        return Err("C did not create a message".to_owned());
    }
    let input = Record(
        record
            .value("input")?
            .as_object()
            .ok_or("`input` is not an object")?,
    );
    let payload = input.hex("payload_hex")?;
    let mut message = GsmtapMessage::um(&payload);
    match record.str("api")? {
        "gsmtap_makemsg_ex" => message.message_type = narrow(input.uint("type")?)?,
        "gsmtap_makemsg" => {}
        other => return Err(format!("unsupported API `{other}`")),
    }
    message.arfcn = narrow(input.uint("arfcn")?)?;
    message.timeslot = narrow(input.uint("timeslot")?)?;
    message.channel_type = narrow(input.uint("channel_type")?)?;
    message.sub_slot = narrow(input.uint("sub_slot")?)?;
    message.frame_number = narrow(input.uint("frame_number")?)?;
    message.signal_dbm = narrow(input.int("signal_dbm")?)?;
    message.snr_db = narrow(input.int("snr_db")?)?;

    let encoded = message.encode();
    let mut check = Check::default();
    check.field("length", record.usize("length")?, encoded.len());
    check.bytes("encoded_hex", &record.hex("encoded_hex")?, &encoded);
    Ok(check)
}

fn narrow<T: TryFrom<U>, U: Copy + std::fmt::Display>(value: U) -> Result<T, String> {
    T::try_from(value).map_err(|_| format!("input value {value} does not fit its C type"))
}
