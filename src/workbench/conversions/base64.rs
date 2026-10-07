use super::*;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Base64Request {
    operation: String,
    source: Source,
    destination: Option<Destination>,
    options: Base64Options,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
struct Base64Options {
    #[serde(default)]
    size_probe: bool,
    #[serde(default)]
    initial_output_length: u32,
}

pub async fn base64(
    Json(request): Json<Base64Request>,
) -> Result<Json<ConversionResult>, ApiError> {
    use libosmocore_rs::base64::{
        osmo_base64_decode, osmo_base64_encode, BASE64_BUFFER_TOO_SMALL, BASE64_INVALID_INPUT,
    };
    let operation = match request.operation.as_str() {
        "encode" => "encode",
        "decode" => "decode",
        _ => return Err(bad_request("Unsupported Base64 operation.")),
    };
    let probe = request.options.size_probe;
    if probe && (operation != "decode" || request.destination.is_some()) {
        return Err(bad_request(
            "Size probes require decode with no destination.",
        ));
    }
    let source = request.source.bytes()?;
    let mut destination = if probe {
        Vec::new()
    } else {
        request
            .destination
            .ok_or_else(|| bad_request("Conversion requires a destination."))?
            .bytes()?
    };
    let initial_destination_hex = hex(&destination);
    let mut length = request.options.initial_output_length as usize;
    let code = if operation == "encode" {
        osmo_base64_encode(&mut destination, &mut length, &source)
    } else {
        osmo_base64_decode(
            if probe { None } else { Some(&mut destination) },
            &mut length,
            &source,
        )
    };
    // Empty/whitespace-only decode preserves the caller's length variable.
    // It is not a byte count and may exceed the destination capacity.
    let written = if operation == "decode"
        && source
            .iter()
            .all(|byte| matches!(byte, b' ' | b'\r' | b'\n'))
    {
        0
    } else {
        length
    };
    let output = if code == 0 && !probe {
        destination.get(..written)
    } else {
        None
    };
    Ok(Json(ConversionResult {
        operation,
        success: code == 0,
        return_value: Some(i64::from(code)),
        output_length: Some(length),
        output_hex: output.map(hex),
        output_text: output.and_then(|bytes| String::from_utf8(bytes.to_vec()).ok()),
        initial_destination_hex,
        final_destination_hex: hex(&destination),
        size_probe: Some(probe),
        error: match code {
            BASE64_BUFFER_TOO_SMALL => Some(ConversionError {
                code: "bufferTooSmall",
                message: "The output-length value gives the required destination capacity.",
            }),
            BASE64_INVALID_INPUT => Some(ConversionError {
                code: "invalidInput",
                message:
                    "Invalid Base64 input. The destination and output-length value are preserved.",
            }),
            _ => None,
        },
    }))
}
