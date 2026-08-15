use super::header::BASE_HEADER_LENGTH;
use super::{EncodeError, GsmtapHeader};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct GsmtapEncodeInput<'a> {
    header: GsmtapHeader,
    extension: &'a [u8],
    payload: &'a [u8],
}
impl<'a> GsmtapEncodeInput<'a> {
    pub fn new(
        header: GsmtapHeader,
        extension: &'a [u8],
        payload: &'a [u8],
    ) -> Result<Self, EncodeError> {
        if extension.len() % 4 != 0 {
            return Err(EncodeError::InvalidExtensionLength {
                length: extension.len(),
            });
        }
        let expected = BASE_HEADER_LENGTH + extension.len();
        let declared = header.header_length();
        if declared != expected {
            return Err(EncodeError::InvalidHeaderLength { declared, expected });
        }
        Ok(Self {
            header,
            extension,
            payload,
        })
    }
    pub fn encode(&self) -> Vec<u8> {
        let h = &self.header;
        let mut out = Vec::with_capacity(h.header_length() + self.payload.len());
        out.extend_from_slice(&[
            h.version(),
            h.header_length_words(),
            h.message_type(),
            h.timeslot(),
        ]);
        out.extend_from_slice(&h.arfcn().to_be_bytes());
        out.extend_from_slice(&[h.signal_dbm() as u8, h.snr_db() as u8]);
        out.extend_from_slice(&h.frame_number().to_be_bytes());
        out.extend_from_slice(&[h.subtype(), h.antenna_number(), h.sub_slot(), h.reserved()]);
        out.extend_from_slice(self.extension);
        out.extend_from_slice(self.payload);
        out
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_mismatched_header_and_extension_without_mutation() {
        let header = GsmtapHeader::new(2, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        assert_eq!(
            GsmtapEncodeInput::new(header, &[], &[]),
            Err(EncodeError::InvalidHeaderLength {
                declared: 20,
                expected: 16
            })
        );
        assert_eq!(
            GsmtapEncodeInput::new(header, &[1, 2, 3], &[]),
            Err(EncodeError::InvalidExtensionLength { length: 3 })
        );
    }
}
