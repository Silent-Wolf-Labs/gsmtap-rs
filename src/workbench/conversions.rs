//! Bounded HTTP adapters for the sibling crate's conversion APIs.

use axum::{http::StatusCode, Json};
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

use super::dto::{hex, parse_hex};

pub const MAX_BUFFER_BYTES: usize = 65_536;
const MAX_SOURCE_BYTES: usize = MAX_BUFFER_BYTES * 3;
pub const MAX_REQUEST_BYTES: usize = 2 * 1024 * 1024;
type ApiError = (StatusCode, String);

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Source {
    encoding: String,
    data: String,
}

#[derive(Deserialize)]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
struct Destination {
    capacity: usize,
    #[serde(default)]
    fill_byte: u8,
    #[serde(default)]
    initial_hex: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ConversionResult {
    operation: &'static str,
    success: bool,
    return_value: Option<i64>,
    output_length: Option<usize>,
    output_hex: Option<String>,
    output_text: Option<String>,
    initial_destination_hex: String,
    final_destination_hex: String,
    error: Option<ConversionError>,
    #[serde(skip_serializing_if = "Option::is_none")]
    size_probe: Option<bool>,
}

#[derive(Serialize)]
struct ConversionError {
    code: &'static str,
    message: &'static str,
}

fn bad_request(message: impl Into<String>) -> ApiError {
    (StatusCode::BAD_REQUEST, message.into())
}

impl Source {
    fn bytes(self) -> Result<Vec<u8>, ApiError> {
        if self.data.len() > MAX_SOURCE_BYTES * 3 {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Source input is too large.".into(),
            ));
        }
        let bytes = match self.encoding.as_str() {
            "text" => self.data.into_bytes(),
            "hex" => parse_hex(&self.data).map_err(bad_request)?,
            _ => return Err(bad_request("Source format must be text or hex.")),
        };
        if bytes.len() > MAX_SOURCE_BYTES {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Source bytes exceed the conversion limit.".into(),
            ));
        }
        Ok(bytes)
    }
}

impl Destination {
    fn bytes(self) -> Result<Vec<u8>, ApiError> {
        if self.capacity > MAX_BUFFER_BYTES {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Destination capacity exceeds 65536 bytes.".into(),
            ));
        }
        if self.initial_hex.is_empty() {
            return Ok(vec![self.fill_byte; self.capacity]);
        }
        if self.initial_hex.len() > MAX_BUFFER_BYTES * 3 {
            return Err((
                StatusCode::PAYLOAD_TOO_LARGE,
                "Initial destination input is too large.".into(),
            ));
        }
        let bytes = parse_hex(&self.initial_hex).map_err(bad_request)?;
        if bytes.len() != self.capacity {
            return Err(bad_request(
                "Initial bytes must exactly match destination capacity.",
            ));
        }
        Ok(bytes)
    }
}

pub mod base64;
pub mod bcd;
pub mod bits;
pub mod hexparse;
