//! Interoperability between osmocore-rs's GSMTAP encoder and this crate's codec.
//!
//! The C-recorded packets in `tests/vectors/` anchor both encoders; the
//! property test extends the comparison to arbitrary header fields.
#![cfg(feature = "osmocore")]

use std::fs;
use std::path::Path;

use gsmtap_rs::gsmtap::{GSMTAP_VERSION, GsmtapEncodeInput, GsmtapHeader, parse};
use osmocore_rs::gsmtap::GsmtapMessage;
use proptest::prelude::*;

fn decode_hex(hex: &str) -> Vec<u8> {
    (0..hex.len())
        .step_by(2)
        .map(|index| u8::from_str_radix(&hex[index..index + 2], 16).unwrap())
        .collect()
}

/// Parses an osmocore-rs packet, checks every field, and re-encodes it here.
fn assert_interoperable(message: GsmtapMessage<'_>) -> Vec<u8> {
    let encoded = message.encode();
    let packet = parse(&encoded).expect("osmocore-rs packet must parse");
    let header = packet.header();
    assert_eq!(header.version(), GSMTAP_VERSION);
    assert_eq!(header.header_length_words(), 4);
    assert_eq!(header.message_type(), message.message_type);
    assert_eq!(header.timeslot(), message.timeslot);
    assert_eq!(header.arfcn(), message.arfcn);
    assert_eq!(header.signal_dbm(), message.signal_dbm);
    assert_eq!(header.snr_db(), message.snr_db);
    assert_eq!(header.frame_number(), message.frame_number);
    assert_eq!(header.subtype(), message.channel_type);
    assert_eq!(header.antenna_number(), 0);
    assert_eq!(header.sub_slot(), message.sub_slot);
    assert_eq!(header.reserved(), 0);
    assert!(packet.extension().is_empty());
    assert_eq!(packet.payload(), message.payload);

    let reencoded = GsmtapEncodeInput::new(
        GsmtapHeader::new(
            GSMTAP_VERSION,
            4,
            message.message_type,
            message.timeslot,
            message.arfcn,
            message.signal_dbm,
            message.snr_db,
            message.frame_number,
            message.channel_type,
            0,
            message.sub_slot,
            0,
        ),
        &[],
        message.payload,
    )
    .expect("a base header must compose")
    .encode();
    assert_eq!(reencoded, encoded, "encoders differ");
    encoded
}

#[test]
fn both_encoders_reproduce_every_c_recorded_packet() {
    let vector_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/vectors");
    let mut cases = 0;
    for entry in fs::read_dir(vector_dir).unwrap() {
        let path = entry.unwrap().path();
        if path.extension().and_then(|extension| extension.to_str()) != Some("json") {
            continue;
        }
        let vector: serde_json::Value =
            serde_json::from_str(&fs::read_to_string(&path).unwrap()).unwrap();
        let input = &vector["input"];
        let payload = decode_hex(input["payload_hex"].as_str().unwrap());
        let mut message = GsmtapMessage::um(&payload);
        if vector["api"] == "gsmtap_makemsg_ex" {
            message.message_type = input["type"].as_u64().unwrap() as u8;
        }
        message.arfcn = input["arfcn"].as_u64().unwrap() as u16;
        message.timeslot = input["timeslot"].as_u64().unwrap() as u8;
        message.channel_type = input["channel_type"].as_u64().unwrap() as u8;
        message.sub_slot = input["sub_slot"].as_u64().unwrap() as u8;
        message.frame_number = input["frame_number"].as_u64().unwrap() as u32;
        message.signal_dbm = input["signal_dbm"].as_i64().unwrap() as i8;
        message.snr_db = input["snr_db"].as_i64().unwrap() as i8;

        let encoded = assert_interoperable(message);
        assert_eq!(
            encoded,
            decode_hex(vector["encoded_hex"].as_str().unwrap()),
            "osmocore-rs differs from C for {}",
            path.display()
        );
        cases += 1;
    }
    assert_eq!(cases, 7);
}

proptest! {
    #[test]
    fn osmocore_packets_parse_and_reencode_identically(
        message_type in any::<u8>(),
        arfcn in any::<u16>(),
        timeslot in any::<u8>(),
        channel_type in any::<u8>(),
        sub_slot in any::<u8>(),
        frame_number in any::<u32>(),
        signal_dbm in any::<i8>(),
        snr_db in any::<i8>(),
        payload in prop::collection::vec(any::<u8>(), 0..64),
    ) {
        assert_interoperable(GsmtapMessage {
            message_type,
            arfcn,
            timeslot,
            channel_type,
            sub_slot,
            frame_number,
            signal_dbm,
            snr_db,
            payload: &payload,
        });
    }
}
