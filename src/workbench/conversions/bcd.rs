use super::*;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct BcdRequest {
    operation: String,
    source: Source,
    destination: Option<Destination>,
    #[serde(default)]
    options: BTreeMap<String, serde_json::Value>,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
struct BcdOptions {
    start_nibble: usize,
    end_nibble: Option<usize>,
    #[serde(default)]
    allow_hex: bool,
}

pub async fn bcd(Json(request): Json<BcdRequest>) -> Result<Json<ConversionResult>, ApiError> {
    use libosmocore_rs::bcd::{osmo_bcd2char, osmo_bcd2str, osmo_char2bcd, osmo_str2bcd, BcdError};
    let operation = match request.operation.as_str() {
        "char2bcd" => "char2bcd",
        "bcd2char" => "bcd2char",
        "str2bcd" => "str2bcd",
        "bcd2str" => "bcd2str",
        _ => return Err(bad_request("Unsupported BCD operation.")),
    };
    let source = request.source.bytes()?;
    if matches!(operation, "char2bcd" | "bcd2char") {
        if source.len() != 1 || request.destination.is_some() || !request.options.is_empty() {
            return Err(bad_request(
                "Scalar BCD operations require one source byte, no destination, and empty options.",
            ));
        }
        let value = if operation == "char2bcd" {
            osmo_char2bcd(source[0])
        } else {
            osmo_bcd2char(source[0])
                .ok_or_else(|| bad_request("BCD nibble must be between 00 and 0F."))?
        };
        return Ok(Json(ConversionResult {
            size_probe: None,
            operation,
            success: true,
            return_value: Some(i64::from(value)),
            output_length: None,
            output_hex: Some(hex(&[value])),
            output_text: (operation == "bcd2char").then(|| char::from(value).to_string()),
            initial_destination_hex: String::new(),
            final_destination_hex: String::new(),
            error: None,
        }));
    }
    let options: BcdOptions =
        serde_json::from_value(serde_json::to_value(request.options).expect("options serialize"))
            .map_err(|error| bad_request(format!("Invalid BCD options: {error}")))?;
    if options.start_nibble > MAX_BUFFER_BYTES * 2
        || options
            .end_nibble
            .is_some_and(|end| end > MAX_BUFFER_BYTES * 2)
    {
        return Err(bad_request("Nibble offsets must be between 0 and 131072."));
    }
    if operation == "bcd2str" && options.end_nibble.is_none() {
        return Err(bad_request("BCD decoding requires an end nibble."));
    }
    let mut destination = request
        .destination
        .ok_or_else(|| bad_request("BCD buffer operations require a destination."))?
        .bytes()?;
    let initial_destination_hex = hex(&destination);
    let converted = if operation == "str2bcd" {
        osmo_str2bcd(
            &mut destination,
            &source,
            options.start_nibble,
            options.end_nibble,
            options.allow_hex,
        )
    } else {
        osmo_bcd2str(
            &mut destination,
            &source,
            options.start_nibble,
            options.end_nibble.unwrap(),
            options.allow_hex,
        )
    };
    let decoded = if operation == "bcd2str"
        && (converted.is_ok() || converted == Err(BcdError::InvalidDigit))
    {
        // The reported length can exceed capacity. Read only the initialized
        // destination through its NUL terminator, never slice by return value.
        let end = destination
            .iter()
            .position(|&byte| byte == 0)
            .unwrap_or(destination.len());
        Some(&destination[..end])
    } else {
        None
    };
    let error = converted.err().map(|error| match error {
        BcdError::InvalidDigit => ConversionError { code: "invalidDigit", message: "An invalid or disallowed digit was encountered. The final buffer preserves completed writes." },
        BcdError::NoSpace => ConversionError { code: "noSpace", message: "Destination buffer is too small or empty." },
        BcdError::OutOfRange => ConversionError { code: "outOfRange", message: "The nibble range is outside the available source or destination." },
    });
    Ok(Json(ConversionResult {
        size_probe: None,
        operation,
        success: converted.is_ok(),
        return_value: converted.ok().map(|value| value as i64),
        output_length: None,
        output_hex: decoded
            .map(hex)
            .or_else(|| (operation == "str2bcd" && converted.is_ok()).then(|| hex(&destination))),
        output_text: decoded.and_then(|bytes| String::from_utf8(bytes.to_vec()).ok()),
        initial_destination_hex,
        final_destination_hex: hex(&destination),
        error,
    }))
}
