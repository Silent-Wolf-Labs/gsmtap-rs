//! GSMTAP parsing and encoding primitives.
//!
//! Encoder byte-layout conformance is derived from `libosmocore`. Decoder
//! validation is an explicit `gsmtap-rs` API contract.

use std::error::Error;
use std::fmt;

/// GSMTAP version supported by the base-header parser.
pub const GSMTAP_VERSION: u8 = 2;
const BASE_HEADER_LENGTH: usize = 16;

/// A parsed GSMTAP base header.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GsmtapHeader {
    version: u8,
    header_length_words: u8,
    message_type: u8,
    timeslot: u8,
    arfcn: u16,
    signal_dbm: i8,
    snr_db: i8,
    frame_number: u32,
    subtype: u8,
    antenna_number: u8,
    sub_slot: u8,
    reserved: u8,
}

impl GsmtapHeader {
    pub fn version(&self) -> u8 {
        self.version
    }

    pub fn header_length_words(&self) -> u8 {
        self.header_length_words
    }

    pub fn header_length(&self) -> usize {
        usize::from(self.header_length_words) * 4
    }

    pub fn message_type(&self) -> u8 {
        self.message_type
    }

    pub fn timeslot(&self) -> u8 {
        self.timeslot
    }

    pub fn arfcn(&self) -> u16 {
        self.arfcn
    }

    pub fn signal_dbm(&self) -> i8 {
        self.signal_dbm
    }

    pub fn snr_db(&self) -> i8 {
        self.snr_db
    }

    pub fn frame_number(&self) -> u32 {
        self.frame_number
    }

    pub fn subtype(&self) -> u8 {
        self.subtype
    }

    pub fn antenna_number(&self) -> u8 {
        self.antenna_number
    }

    pub fn sub_slot(&self) -> u8 {
        self.sub_slot
    }

    pub fn reserved(&self) -> u8 {
        self.reserved
    }
}

/// A borrowed GSMTAP packet. Payload begins at the declared header boundary.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GsmtapPacket<'a> {
    header: GsmtapHeader,
    extension: &'a [u8],
    payload: &'a [u8],
}

impl<'a> GsmtapPacket<'a> {
    pub fn header(&self) -> &GsmtapHeader {
        &self.header
    }

    /// Bytes after the 16-byte base header and before the payload.
    pub fn extension(&self) -> &'a [u8] {
        self.extension
    }

    pub fn payload(&self) -> &'a [u8] {
        self.payload
    }
}

/// Errors produced by [`parse`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ParseError {
    Truncated { needed: usize, actual: usize },
    InvalidHeaderLength { words: u8 },
    UnsupportedVersion { version: u8 },
}

impl fmt::Display for ParseError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Truncated { needed, actual } => {
                write!(
                    formatter,
                    "truncated GSMTAP packet: need {needed} bytes, got {actual}"
                )
            }
            Self::InvalidHeaderLength { words } => {
                write!(formatter, "invalid GSMTAP header length: {words} words")
            }
            Self::UnsupportedVersion { version } => {
                write!(formatter, "unsupported GSMTAP version: {version}")
            }
        }
    }
}

impl Error for ParseError {}

/// Parses a GSMTAP v2 packet without allocating.
///
/// Unknown message and subtype values are preserved. The payload begins at the
/// declared `header_length_words * 4` boundary, after any extension bytes.
pub fn parse(input: &[u8]) -> Result<GsmtapPacket<'_>, ParseError> {
    if input.len() < 2 {
        return Err(ParseError::Truncated {
            needed: 2,
            actual: input.len(),
        });
    }

    let header_length_words = input[1];
    if header_length_words < 4 {
        return Err(ParseError::InvalidHeaderLength {
            words: header_length_words,
        });
    }
    let header_length = header_length_words as usize * 4;
    if input.len() < header_length {
        return Err(ParseError::Truncated {
            needed: header_length,
            actual: input.len(),
        });
    }
    if input[0] != GSMTAP_VERSION {
        return Err(ParseError::UnsupportedVersion { version: input[0] });
    }

    Ok(GsmtapPacket {
        header: GsmtapHeader {
            version: input[0],
            header_length_words,
            message_type: input[2],
            timeslot: input[3],
            arfcn: u16::from_be_bytes([input[4], input[5]]),
            signal_dbm: input[6] as i8,
            snr_db: input[7] as i8,
            frame_number: u32::from_be_bytes([input[8], input[9], input[10], input[11]]),
            subtype: input[12],
            antenna_number: input[13],
            sub_slot: input[14],
            reserved: input[15],
        },
        extension: &input[BASE_HEADER_LENGTH..header_length],
        payload: &input[header_length..],
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_base_header_and_empty_payload() {
        let packet = parse(&[
            2, 4, 0xfe, 7, 0x12, 0x34, 0xb7, 19, 1, 2, 3, 4, 0xff, 2, 1, 9,
        ])
        .unwrap();
        assert_eq!(packet.header().message_type(), 0xfe);
        assert_eq!(packet.header().arfcn(), 0x1234);
        assert_eq!(packet.header().signal_dbm(), -73);
        assert_eq!(packet.header().frame_number(), 0x0102_0304);
        assert!(packet.extension().is_empty());
        assert!(packet.payload().is_empty());
    }

    #[test]
    fn payload_starts_after_declared_extension_not_offset_sixteen() {
        let input = [
            2, 5, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xde, 0xad, 0xbe,
            0xef, // plausible payload, but declared extension
            0xca, 0xfe,
        ];
        let packet = parse(&input).unwrap();
        assert_eq!(packet.extension(), &[0xde, 0xad, 0xbe, 0xef]);
        assert_eq!(packet.payload(), &[0xca, 0xfe]);
    }

    #[test]
    fn rejects_truncated_and_invalid_headers() {
        assert_eq!(
            parse(&[]),
            Err(ParseError::Truncated {
                needed: 2,
                actual: 0
            })
        );
        assert_eq!(
            parse(&[2]),
            Err(ParseError::Truncated {
                needed: 2,
                actual: 1
            })
        );
        for words in 0..4 {
            assert_eq!(
                parse(&[2, words]),
                Err(ParseError::InvalidHeaderLength { words })
            );
        }
        assert_eq!(
            parse(&[2, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
            Err(ParseError::Truncated {
                needed: 20,
                actual: 16
            })
        );
    }

    #[test]
    fn rejects_unsupported_version_after_structural_validation() {
        let mut input = [0_u8; 16];
        input[0] = 1;
        input[1] = 4;
        assert_eq!(
            parse(&input),
            Err(ParseError::UnsupportedVersion { version: 1 })
        );
    }
}
