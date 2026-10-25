//! Convolutional code traces against `osmocore_rs::conv`.
//!
//! The fixture records descriptor (`code`) records and ordered operation
//! traces for four C backends. Encoder and decoder sessions are keyed by
//! backend, code and case, and replayed in file order. After each step the
//! recorded encoder or decoder state is compared with osmocore-rs's state.

use std::collections::HashMap;

use osmocore_rs::compat::conv::{
    diagnostics, osmo_conv_decode_acc, osmo_conv_decode_scan, osmo_conv_get_input_length,
    osmo_conv_get_output_length,
};
use osmocore_rs::conv::{
    ConvCode, ConvCodeSpec, ConvDecoder, ConvEncoder, ConvError, Termination, decode, encode,
};
use serde_json::{Map, Value};

use super::record::{self, Record};
use super::{Check, ModuleRun, Outcome};

const FIXTURE: &str = "conv/conv_vectors.ndjson";
const EINVAL: i64 = -22;
const EPROTO: i64 = -71;

type Key<'a> = (&'a str, &'a str, &'a str);

pub(super) fn verify(run: &mut ModuleRun<'_>) -> Result<(), String> {
    let text = run.fixture(FIXTURE)?;
    let records = record::lines(&text).map_err(|error| format!("{FIXTURE}: {error}"))?;

    let mut codes = HashMap::new();
    for map in &records {
        let record = Record(map);
        match record.str("type")? {
            "meta" => run.revision(record.str("upstream_git_revision")?),
            "code" => {
                let key = (record.str("backend")?, record.str("id")?);
                codes.insert(key, descriptor(&record));
            }
            _ => {}
        }
    }

    let mut sessions = Sessions::default();
    for map in &records {
        let record = Record(map);
        if record.str("type")? != "op" {
            continue;
        }
        let case = format!(
            "{}/{}#{}",
            record.str("backend").unwrap_or("?"),
            record.str("case").unwrap_or("?"),
            record.int("seq").unwrap_or(-1),
        );
        let outcome = match operation(map, &codes, &mut sessions) {
            Err(reason) => Outcome::Unsupported(reason),
            Ok(outcome) => outcome,
        };
        run.record(case, outcome);
    }
    Ok(())
}

fn descriptor(record: &Record<'_>) -> Result<ConvCode, String> {
    let pairs = |key: &str| -> Result<Vec<[u8; 2]>, String> {
        let bytes = record.hex(key)?;
        let (pairs, []) = bytes.as_chunks::<2>() else {
            return Err(format!("`{key}` has an odd length"));
        };
        Ok(pairs.to_vec())
    };
    let optional = |key: &str| -> Result<Option<Vec<u8>>, String> {
        let bytes = record.hex(key)?;
        Ok((!bytes.is_empty()).then_some(bytes))
    };
    let puncture = if record.uint("puncture_present")? == 0 {
        None
    } else {
        Some(
            record
                .list("puncture")?
                .into_iter()
                .map(|position| i32::try_from(position).map_err(|_| "puncture position"))
                .collect::<Result<_, _>>()?,
        )
    };
    let spec = ConvCodeSpec {
        n: record.i32("n")?,
        k: record.i32("k")?,
        len: record.i32("len")?,
        term: Termination(record.i32("term")?),
        next_output: pairs("next_output")?,
        next_state: pairs("next_state")?,
        next_term_output: optional("next_term_output")?,
        next_term_state: optional("next_term_state")?,
        puncture,
    };
    ConvCode::new(spec).map_err(|error| format!("osmocore-rs rejected the descriptor: {error:?}"))
}

#[derive(Default)]
struct Sessions<'c> {
    encoders: HashMap<Key<'c>, ConvEncoder<'c>>,
    decoders: HashMap<Key<'c>, ConvDecoder<'c>>,
}

fn operation<'c>(
    map: &'c Map<String, Value>,
    codes: &'c HashMap<(&'c str, &'c str), Result<ConvCode, String>>,
    sessions: &mut Sessions<'c>,
) -> Result<Outcome, String> {
    let record = Record(map);
    let backend = record.str("backend")?;
    let code_id = record.str("code")?;
    let key = (backend, code_id, record.str("case")?);
    let code = match codes.get(&(backend, code_id)) {
        None => return Err(format!("no descriptor `{code_id}` for {backend}")),
        Some(Err(reason)) => {
            let mut check = Check::default();
            check.rust_only("descriptor", "accepted", reason);
            return Ok(Outcome::from(Ok(check)));
        }
        Some(Ok(code)) => code,
    };
    let api = record.str("api")?;
    let mut check = Check::default();
    match api {
        "get_input_length" | "get_output_length" => {
            let argument = record.i32("length_arg")?;
            let length = if api == "get_input_length" {
                osmo_conv_get_input_length(code, argument)
            } else {
                osmo_conv_get_output_length(code, argument)
            };
            status(
                &mut check,
                record.int("ret")?,
                length.map(i64::from),
                |_| None,
            );
        }
        "encode" => {
            let (input, mut output) = buffers(&record)?;
            let result = encode(code, &input, &mut output).map(|count| count as i64);
            status(&mut check, record.int("ret")?, result, |_| None);
            compare_buffers(&mut check, &record, &input, &output)?;
        }
        "encode_init" => {
            match sessions.encoders.get_mut(&key) {
                Some(encoder) => encoder.reset(),
                None => {
                    sessions.encoders.insert(key, ConvEncoder::new(code));
                }
            }
            encoder_state(&mut check, &record, &sessions.encoders[&key])?;
        }
        "encode_load_state" | "encode_raw" | "encode_flush" => {
            let encoder = sessions
                .encoders
                .get_mut(&key)
                .ok_or("no encoder session for this case")?;
            let (input, mut output) = buffers(&record)?;
            let result = match api {
                "encode_load_state" => encoder.load_state(&input).map(|()| 0),
                "encode_raw" => encoder.encode_raw(&input, &mut output),
                _ => encoder.flush(&mut output),
            };
            status(
                &mut check,
                record.int("ret")?,
                result.map(|count| count as i64),
                |_| None,
            );
            compare_buffers(&mut check, &record, &input, &output)?;
            encoder_state(&mut check, &record, encoder)?;
        }
        "decode" | "decode_acc" => {
            let (input, mut output) = buffers(&record)?;
            let symbols = soft(&input);
            let result = if api == "decode" {
                decode(code, &symbols, &mut output)
            } else {
                osmo_conv_decode_acc(code, &symbols, &mut output)
            };
            status(
                &mut check,
                record.int("ret")?,
                result.map(i64::from),
                |error| match error {
                    ConvError::InvalidRate
                    | ConvError::InvalidConstraintLength
                    | ConvError::InvalidFrameLength => Some(EINVAL),
                    ConvError::UnsupportedRecursiveCode => Some(EPROTO),
                    _ => None,
                },
            );
            compare_buffers(&mut check, &record, &input, &output)?;
        }
        "decode_init" => {
            match ConvDecoder::new(code, record.i32("length_arg")?, record.i32("start_state")?) {
                Ok(decoder) => {
                    if sessions.decoders.insert(key, decoder).is_some() {
                        return Err("decoder session initialized twice".to_owned());
                    }
                    decoder_state(&mut check, &record, &sessions.decoders[&key])?;
                }
                Err(error) => check.rust_only("init", "initialized", error),
            }
        }
        "decode_deinit" => {
            // Rust releases decoder storage when the session is dropped.
            sessions
                .decoders
                .remove(&key)
                .ok_or("no decoder session for this case")?;
            check.field("storage_released", record.uint("storage_released")?, 1);
        }
        _ => {
            let decoder = sessions
                .decoders
                .get_mut(&key)
                .ok_or("no decoder session for this case")?;
            // C returns -1 when no path survives.
            let no_survivor = |error| (error == ConvError::NoSurvivor).then_some(-1);
            match api {
                "decode_reset" => {
                    let result = decoder.reset(record.i32("start_state")?);
                    status(&mut check, 0, result.map(|()| 0), |_| None);
                }
                "decode_rewind" => {
                    status(&mut check, 0, decoder.rewind().map(|()| 0), |_| None);
                }
                "decode_scan" | "decode_flush" => {
                    let input = record.hex("input_before")?;
                    let symbols = soft(&input);
                    let result = if api == "decode_scan" {
                        osmo_conv_decode_scan(decoder, &symbols, record.i32("n")?)
                    } else {
                        decoder.flush(&symbols)
                    };
                    status(
                        &mut check,
                        record.int("ret")?,
                        result.map(|n| n as i64),
                        |_| None,
                    );
                    check.bytes("input_after", &record.hex("input_after")?, &input);
                }
                "decode_get_best_end_state" => {
                    let result = decoder.best_end_state().map(|state| state as i64);
                    status(&mut check, record.int("ret")?, result, no_survivor);
                }
                "decode_get_output" => {
                    let mut output = record.hex("output_before")?;
                    let result = decoder.get_output(
                        &mut output,
                        record.uint("has_flush")? != 0,
                        record.i32("end_state")?,
                    );
                    status(
                        &mut check,
                        record.int("ret")?,
                        result.map(i64::from),
                        no_survivor,
                    );
                    check.bytes("output_after", &record.hex("output_after")?, &output);
                }
                other => return Err(format!("unsupported API `{other}`")),
            }
            decoder_state(&mut check, &record, decoder)?;
        }
    }
    Ok(Outcome::from(Ok(check)))
}

/// Compares a C return value with a Rust result, mapping errors with C codes.
fn status(
    check: &mut Check,
    expected: i64,
    actual: Result<i64, ConvError>,
    c_code: impl Fn(ConvError) -> Option<i64>,
) {
    match actual {
        Ok(value) => check.field("ret", expected, value),
        Err(error) => match c_code(error) {
            Some(value) => check.field("ret", expected, value),
            None => check.rust_only("ret", expected, error),
        },
    }
}

fn buffers(record: &Record<'_>) -> Result<(Vec<u8>, Vec<u8>), String> {
    Ok((record.hex("input_before")?, record.hex("output_before")?))
}

fn compare_buffers(
    check: &mut Check,
    record: &Record<'_>,
    input: &[u8],
    output: &[u8],
) -> Result<(), String> {
    check.bytes("input_after", &record.hex("input_after")?, input);
    check.bytes("output_after", &record.hex("output_after")?, output);
    Ok(())
}

/// Soft-decision symbols are recorded as raw `int8_t` bytes.
fn soft(bytes: &[u8]) -> Vec<i8> {
    bytes.iter().map(|&byte| byte as i8).collect()
}

fn encoder_state(
    check: &mut Check,
    record: &Record<'_>,
    encoder: &ConvEncoder<'_>,
) -> Result<(), String> {
    if record.0.contains_key("state") {
        check.field("state", record.uint("state")?, u64::from(encoder.state()));
        check.field("i_idx", record.usize("i_idx")?, encoder.input_index());
        check.field("p_idx", record.usize("p_idx")?, encoder.puncture_index());
    }
    Ok(())
}

fn decoder_state(
    check: &mut Check,
    record: &Record<'_>,
    decoder: &ConvDecoder<'_>,
) -> Result<(), String> {
    let state = diagnostics(decoder);
    let widen = |values: &[u32]| values.iter().map(|&value| i64::from(value)).collect();
    check.field("len", record.usize("len")?, decoder.frame_length());
    check.field("n_states", record.usize("n_states")?, decoder.state_count());
    check.field("o_idx", record.usize("o_idx")?, state.output_index);
    check.field("p_idx", record.usize("p_idx")?, state.puncture_index);
    check.field::<Vec<i64>>("ae", record.list("ae")?, widen(state.accumulated_errors));
    let next = match record.uint("ae_next_valid")? {
        0 => None,
        _ => Some(record.list("ae_next")?),
    };
    check.field("ae_next", next, state.next_accumulated_errors.map(widen));
    let history = state
        .predecessor_history
        .iter()
        .map(|state| state.map_or(-1, i64::from))
        .collect();
    check.field::<Vec<i64>>("history", record.list("history")?, history);
    Ok(())
}
