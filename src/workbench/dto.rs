use serde::{Deserialize, Serialize};

use crate::gsmtap::GsmtapPacket;

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PacketRecord {
    pub id: u64,
    pub direction: &'static str,
    pub mode: String,
    pub timestamp_ms: u128,
    pub peer: String,
    pub destination: Option<String>,
    pub raw_hex: String,
    pub decoded: Option<DecodedPacket>,
    pub parse_error: Option<String>,
    pub forward_status: Option<String>,
    pub modified: bool,
    pub original_raw_hex: Option<String>,
    pub final_raw_hex: Option<String>,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DecodedPacket {
    pub version: u8,
    pub header_length_words: u8,
    pub message_type: u8,
    pub timeslot: u8,
    pub arfcn: u16,
    pub signal_dbm: i8,
    pub snr_db: i8,
    pub frame_number: u32,
    pub subtype: u8,
    pub antenna_number: u8,
    pub sub_slot: u8,
    pub reserved: u8,
    pub extension_hex: String,
    pub payload_hex: String,
}

#[derive(Debug, Clone, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EncodeSendRequest {
    pub version: u8,
    pub header_length_words: u8,
    pub message_type: u8,
    pub timeslot: u8,
    pub arfcn: u16,
    pub signal_dbm: i8,
    pub snr_db: i8,
    pub frame_number: u32,
    pub subtype: u8,
    pub antenna_number: u8,
    pub sub_slot: u8,
    pub reserved: u8,
    #[serde(default)]
    pub extension_hex: String,
    #[serde(default)]
    pub payload_hex: String,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SendResponse {
    pub timestamp_ms: u128,
    pub destination: String,
    pub encoded_hex: String,
}

pub fn from_decoded(
    direction: &'static str,
    peer: String,
    raw: &[u8],
    packet: &GsmtapPacket<'_>,
) -> PacketRecord {
    let h = packet.header();
    PacketRecord {
        id: 0,
        direction,
        mode: "listen".into(),
        timestamp_ms: now_ms(),
        peer,
        destination: None,
        raw_hex: hex(raw),
        decoded: Some(DecodedPacket {
            version: h.version(),
            header_length_words: h.header_length_words(),
            message_type: h.message_type(),
            timeslot: h.timeslot(),
            arfcn: h.arfcn(),
            signal_dbm: h.signal_dbm(),
            snr_db: h.snr_db(),
            frame_number: h.frame_number(),
            subtype: h.subtype(),
            antenna_number: h.antenna_number(),
            sub_slot: h.sub_slot(),
            reserved: h.reserved(),
            extension_hex: hex(packet.extension()),
            payload_hex: hex(packet.payload()),
        }),
        parse_error: None,
        forward_status: None,
        modified: false,
        original_raw_hex: None,
        final_raw_hex: None,
    }
}

pub fn from_error(
    direction: &'static str,
    peer: String,
    raw: &[u8],
    error: String,
) -> PacketRecord {
    PacketRecord {
        id: 0,
        direction,
        mode: "listen".into(),
        timestamp_ms: now_ms(),
        peer,
        destination: None,
        raw_hex: hex(raw),
        decoded: None,
        parse_error: Some(error),
        forward_status: None,
        modified: false,
        original_raw_hex: None,
        final_raw_hex: None,
    }
}

pub fn hex(bytes: &[u8]) -> String {
    bytes
        .iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

pub fn parse_hex(input: &str) -> Result<Vec<u8>, String> {
    let compact: String = input.chars().filter(|c| !c.is_ascii_whitespace()).collect();
    if compact.len() % 2 != 0 {
        return Err("hex input must contain complete bytes".into());
    }
    (0..compact.len())
        .step_by(2)
        .map(|index| {
            u8::from_str_radix(&compact[index..index + 2], 16)
                .map_err(|_| format!("invalid hex byte at offset {index}"))
        })
        .collect()
}

fn now_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
}
