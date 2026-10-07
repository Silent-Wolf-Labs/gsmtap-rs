use super::*;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct HexparseRequest {
    operation: String,
    source: Source,
    destination: Destination,
    #[serde(default)]
    options: BTreeMap<String, serde_json::Value>,
}

pub async fn hexparse(
    Json(request): Json<HexparseRequest>,
) -> Result<Json<ConversionResult>, ApiError> {
    if request.operation != "parse" || !request.options.is_empty() {
        return Err(bad_request(
            "Hexparse supports only parse with empty options.",
        ));
    }
    let source = request.source.bytes()?;
    let mut destination = request.destination.bytes()?;
    let initial_destination_hex = hex(&destination);
    let converted = libosmocore_rs::hexparse::osmo_hexparse(&source, &mut destination);
    let return_value = converted.ok();
    let output = return_value.map(|length| &destination[..length]);
    Ok(Json(ConversionResult {
        size_probe: None,
        operation: "parse",
        success: converted.is_ok(),
        return_value: return_value.map(|value| value as i64),
        output_length: None,
        output_hex: output.map(hex),
        output_text: output.and_then(|bytes| String::from_utf8(bytes.to_vec()).ok()),
        initial_destination_hex,
        final_destination_hex: hex(&destination),
        error: converted.err().map(|_| ConversionError {
            code: "invalidInput",
            message: "Parsing failed: invalid hex, an incomplete byte, or insufficient destination capacity. Partial writes are shown below.",
        }),
    }))
}
