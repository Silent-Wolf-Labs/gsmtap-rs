use gsmtap_rs::gsmtap::{parse, GsmtapEncodeInput, GsmtapHeader};
use proptest::prelude::*;

// Rust-owned invariants. C-backed encoder evidence lives in
// `gsmtap_reference_vectors.rs`.
proptest! {
    #[test]
    fn encode_then_parse_preserves_valid_gsmtap_packets(
        message_type in any::<u8>(),
        timeslot in any::<u8>(),
        arfcn in any::<u16>(),
        signal_dbm in any::<i8>(),
        snr_db in any::<i8>(),
        frame_number in any::<u32>(),
        subtype in any::<u8>(),
        antenna_number in any::<u8>(),
        sub_slot in any::<u8>(),
        reserved in any::<u8>(),
        extension_words in 0_u8..=4,
        extension_storage in prop::collection::vec(any::<u8>(), 16),
        payload in prop::collection::vec(any::<u8>(), 0..64),
    ) {
        let extension = &extension_storage[..usize::from(extension_words) * 4];
        let header = GsmtapHeader::new(
            2,
            4 + extension_words,
            message_type,
            timeslot,
            arfcn,
            signal_dbm,
            snr_db,
            frame_number,
            subtype,
            antenna_number,
            sub_slot,
            reserved,
        );

        let encoded = GsmtapEncodeInput::new(header, extension, &payload)
            .unwrap()
            .encode();
        let decoded = parse(&encoded).unwrap();

        prop_assert_eq!(decoded.header(), &header);
        prop_assert_eq!(decoded.extension(), extension);
        prop_assert_eq!(decoded.payload(), payload.as_slice());
        prop_assert_eq!(
            GsmtapEncodeInput::new(*decoded.header(), decoded.extension(), decoded.payload())
                .unwrap()
                .encode(),
            encoded,
        );
    }
}
