use super::header::BASE_HEADER_LENGTH;
use super::{GsmtapHeader, ParseError, GSMTAP_VERSION};

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
    pub fn extension(&self) -> &'a [u8] {
        self.extension
    }
    pub fn payload(&self) -> &'a [u8] {
        self.payload
    }
}

pub fn parse(input: &[u8]) -> Result<GsmtapPacket<'_>, ParseError> {
    if input.len() < 2 {
        return Err(ParseError::Truncated {
            needed: 2,
            actual: input.len(),
        });
    }
    let words = input[1];
    if words < 4 {
        return Err(ParseError::InvalidHeaderLength { words });
    }
    let length = usize::from(words) * 4;
    if input.len() < length {
        return Err(ParseError::Truncated {
            needed: length,
            actual: input.len(),
        });
    }
    if input[0] != GSMTAP_VERSION {
        return Err(ParseError::UnsupportedVersion { version: input[0] });
    }
    Ok(GsmtapPacket {
        header: GsmtapHeader::new(
            input[0],
            words,
            input[2],
            input[3],
            u16::from_be_bytes([input[4], input[5]]),
            input[6] as i8,
            input[7] as i8,
            u32::from_be_bytes([input[8], input[9], input[10], input[11]]),
            input[12],
            input[13],
            input[14],
            input[15],
        ),
        extension: &input[BASE_HEADER_LENGTH..length],
        payload: &input[length..],
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_extension_separate_from_payload() {
        let input = [
            2, 5, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe,
        ];
        let packet = parse(&input).unwrap();
        assert_eq!(packet.extension(), &[0xde, 0xad, 0xbe, 0xef]);
        assert_eq!(packet.payload(), &[0xca, 0xfe]);
    }

    #[test]
    fn rejects_truncated_invalid_and_unsupported_headers() {
        assert_eq!(
            parse(&[]),
            Err(ParseError::Truncated {
                needed: 2,
                actual: 0
            })
        );
        assert_eq!(
            parse(&[2, 3]),
            Err(ParseError::InvalidHeaderLength { words: 3 })
        );
        let mut truncated = [0; 16];
        truncated[0] = 2;
        truncated[1] = 5;
        assert_eq!(
            parse(&truncated),
            Err(ParseError::Truncated {
                needed: 20,
                actual: 16
            })
        );
    }
}
