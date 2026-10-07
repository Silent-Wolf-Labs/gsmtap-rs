use super::*;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct BitsRequest {
    operation: String,
    source: Source,
    destination: Destination,
    options: BTreeMap<String, serde_json::Value>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
struct BitsOptions {
    num_bits: usize,
    input_offset: Option<usize>,
    output_offset: Option<usize>,
    lsb_mode: Option<bool>,
}

pub async fn bits(Json(request): Json<BitsRequest>) -> Result<Json<ConversionResult>, ApiError> {
    use libosmocore_rs::bits::{
        osmo_pbit2ubit, osmo_pbit2ubit_ext, osmo_ubit2pbit, osmo_ubit2pbit_ext, BitError,
    };
    let operation = match request.operation.as_str() {
        "pack" => "pack",
        "unpack" => "unpack",
        "pack-ext" => "pack-ext",
        "unpack-ext" => "unpack-ext",
        _ => return Err(bad_request("Unsupported bit conversion operation.")),
    };
    let extended = operation.ends_with("-ext");
    let unpack = operation.starts_with("unpack");
    let options: BitsOptions =
        serde_json::from_value(serde_json::to_value(request.options).expect("options serialize"))
            .map_err(|error| bad_request(format!("Invalid bit options: {error}")))?;
    if !extended
        && (options.input_offset.is_some()
            || options.output_offset.is_some()
            || options.lsb_mode.is_some())
    {
        return Err(bad_request(
            "Offsets and bit ordering require an extended operation.",
        ));
    }
    let input = options.input_offset.unwrap_or(0);
    let output = options.output_offset.unwrap_or(0);
    if [options.num_bits, input, output]
        .iter()
        .any(|&value| value > MAX_BUFFER_BYTES * 8)
    {
        return Err(bad_request(
            "Bit counts and offsets must be between 0 and 524288.",
        ));
    }
    let source = if request.source.encoding == "bits" {
        if unpack {
            return Err(bad_request(
                "Unpacking requires packed source bytes, not unpacked-bit input.",
            ));
        }
        if request.source.data.len() > MAX_SOURCE_BYTES * 3 {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Bit source input is too large.".into(),
            ));
        }
        let mut bytes = Vec::new();
        for byte in request
            .source
            .data
            .bytes()
            .filter(|byte| !matches!(byte, b' ' | b'\t' | b'\r' | b'\n'))
        {
            match byte {
                b'0' | b'1' => bytes.push(byte - b'0'),
                _ => {
                    return Err(bad_request(
                        "Bit input accepts only 0, 1, and ASCII whitespace.",
                    ))
                }
            }
        }
        if bytes.len() > MAX_SOURCE_BYTES {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Too many source bits.".into(),
            ));
        }
        bytes
    } else {
        request.source.bytes()?
    };
    let mut destination = request.destination.bytes()?;
    let initial_destination_hex = hex(&destination);
    let converted = match operation {
        "pack" => osmo_ubit2pbit(&mut destination, &source, options.num_bits),
        "unpack" => osmo_pbit2ubit(&mut destination, &source, options.num_bits),
        "pack-ext" => osmo_ubit2pbit_ext(
            &mut destination,
            output,
            &source,
            input,
            options.num_bits,
            options.lsb_mode.unwrap_or(false),
        ),
        _ => osmo_pbit2ubit_ext(
            &mut destination,
            output,
            &source,
            input,
            options.num_bits,
            options.lsb_mode.unwrap_or(false),
        ),
    };
    let output_text = if unpack && converted.is_ok() {
        let count = if !extended && options.num_bits == 0 {
            1
        } else {
            options.num_bits
        };
        // Zero-count extended calls may return a position outside the buffer.
        // Neither this range nor packed output is sliced by the API return value.
        if count == 0 {
            Some(String::new())
        } else {
            Some(
                destination[output..output + count]
                    .iter()
                    .map(u8::to_string)
                    .collect::<Vec<_>>()
                    .join(" "),
            )
        }
    } else {
        None
    };
    Ok(Json(ConversionResult {
        size_probe: None,
        operation, success: converted.is_ok(), return_value: converted.ok().map(|value| value as i64), output_length: None,
        output_hex: converted.is_ok().then(|| hex(&destination)), output_text,
        initial_destination_hex, final_destination_hex: hex(&destination),
        error: converted.err().map(|error| match error {
            BitError::InputTooShort => ConversionError { code: "inputTooShort", message: "Source does not contain the requested bit range. Destination is unchanged." },
            BitError::OutputTooShort => ConversionError { code: "outputTooShort", message: "Destination is too small for the requested bit range. Destination is unchanged." },
            BitError::ArithmeticOverflow => ConversionError { code: "arithmeticOverflow", message: "Bit range arithmetic overflowed. Destination is unchanged." },
        }),
    }))
}
