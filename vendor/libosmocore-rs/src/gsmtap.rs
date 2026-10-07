//! GSMTAP v2 packet construction compatible with selected libosmocore APIs.
//!
//! This module implements the observable packet-construction behavior of
//! `gsmtap_makemsg_ex()` and its `gsmtap_makemsg()` UM wrapper. It deliberately
//! does not provide a general GSMTAP parser or define malformed-input behavior.

/// GSMTAP version emitted by the selected libosmocore constructors.
pub const GSMTAP_VERSION: u8 = 2;
/// Size of the GSMTAP v2 base header emitted by the constructors, in bytes.
pub const GSMTAP_HEADER_LENGTH: usize = 16;
/// GSMTAP type used by [`gsmtap_makemsg`].
pub const GSMTAP_TYPE_UM: u8 = 1;

/// Inputs controlled by libosmocore's `gsmtap_makemsg_ex()` constructor.
///
/// `antenna_number` and the reserved header byte are not represented because
/// the C constructor does not accept them; both are always emitted as zero.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GsmtapMessage<'a> {
    pub message_type: u8,
    pub arfcn: u16,
    pub timeslot: u8,
    pub channel_type: u8,
    pub sub_slot: u8,
    pub frame_number: u32,
    pub signal_dbm: i8,
    pub snr_db: i8,
    pub payload: &'a [u8],
}

impl<'a> GsmtapMessage<'a> {
    /// Encodes this input as a GSMTAP v2 packet.
    pub fn encode(self) -> Vec<u8> {
        encode_fields(
            self.message_type,
            self.arfcn,
            self.timeslot,
            self.channel_type,
            self.sub_slot,
            self.frame_number,
            self.signal_dbm,
            self.snr_db,
            self.payload,
        )
    }
}

/// Rust equivalent of libosmocore's `gsmtap_makemsg_ex()`.
pub fn gsmtap_makemsg_ex(
    message_type: u8,
    arfcn: u16,
    timeslot: u8,
    channel_type: u8,
    sub_slot: u8,
    frame_number: u32,
    signal_dbm: i8,
    snr_db: i8,
    payload: &[u8],
) -> Vec<u8> {
    GsmtapMessage {
        message_type,
        arfcn,
        timeslot,
        channel_type,
        sub_slot,
        frame_number,
        signal_dbm,
        snr_db,
        payload,
    }
    .encode()
}

/// Rust equivalent of libosmocore's `gsmtap_makemsg()`.
///
/// This is the UM-default wrapper and delegates to [`gsmtap_makemsg_ex`].
pub fn gsmtap_makemsg(
    arfcn: u16,
    timeslot: u8,
    channel_type: u8,
    sub_slot: u8,
    frame_number: u32,
    signal_dbm: i8,
    snr_db: i8,
    payload: &[u8],
) -> Vec<u8> {
    gsmtap_makemsg_ex(
        GSMTAP_TYPE_UM,
        arfcn,
        timeslot,
        channel_type,
        sub_slot,
        frame_number,
        signal_dbm,
        snr_db,
        payload,
    )
}

fn encode_fields(
    message_type: u8,
    arfcn: u16,
    timeslot: u8,
    channel_type: u8,
    sub_slot: u8,
    frame_number: u32,
    signal_dbm: i8,
    snr_db: i8,
    payload: &[u8],
) -> Vec<u8> {
    let mut packet = Vec::with_capacity(GSMTAP_HEADER_LENGTH + payload.len());
    packet.extend_from_slice(&[
        GSMTAP_VERSION,
        (GSMTAP_HEADER_LENGTH / 4) as u8,
        message_type,
        timeslot,
    ]);
    packet.extend_from_slice(&arfcn.to_be_bytes());
    packet.extend_from_slice(&[signal_dbm as u8, snr_db as u8]);
    packet.extend_from_slice(&frame_number.to_be_bytes());
    packet.extend_from_slice(&[
        channel_type,
        0, // antenna_nr is fixed by the C constructor.
        sub_slot,
        0, // reserved is fixed by the C constructor.
    ]);
    packet.extend_from_slice(payload);
    packet
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rust_owned_empty_payload_has_only_the_base_header() {
        let packet = gsmtap_makemsg_ex(0xff, 0xffff, 0xff, 0xff, 0xff, u32::MAX, -128, 127, &[]);

        assert_eq!(packet.len(), GSMTAP_HEADER_LENGTH);
        assert_eq!(packet[0], GSMTAP_VERSION);
        assert_eq!(packet[1], 4);
        assert_eq!(packet[13], 0);
        assert_eq!(packet[15], 0);
    }

    #[test]
    fn rust_owned_payload_follows_the_complete_header_unchanged() {
        let payload = [0, 1, 2, 255];
        let packet = gsmtap_makemsg_ex(1, 0, 0, 0, 0, 0, 0, 0, &payload);

        assert_eq!(&packet[GSMTAP_HEADER_LENGTH..], payload);
    }

    #[test]
    fn um_wrapper_delegates_to_the_general_constructor() {
        let wrapped = gsmtap_makemsg(42, 3, 8, 1, 0x0102_0304, -73, 19, &[1, 2]);
        let explicit = gsmtap_makemsg_ex(1, 42, 3, 8, 1, 0x0102_0304, -73, 19, &[1, 2]);

        assert_eq!(wrapped, explicit);
    }
}
