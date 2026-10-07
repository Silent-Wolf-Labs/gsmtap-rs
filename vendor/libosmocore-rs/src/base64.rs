//! Buffer-oriented Base64 functions compatible with libosmocore.
//!
//! These functions retain the selected C API's observable contract: callers
//! supply the output buffer and its length is its slice length; `olen` is
//! updated exactly where the reference does. Encoding writes a trailing NUL
//! after its reported output, so a successful non-empty encode needs one more
//! byte than `olen`. `None` models a null decode destination for size probes.

/// C-compatible result for an output buffer that is too small (`-ENOBUFS`).
pub const BASE64_BUFFER_TOO_SMALL: i32 = -105;
/// C-compatible result for malformed encoded input (`-EINVAL`).
pub const BASE64_INVALID_INPUT: i32 = -22;

const ENCODE_MAP: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// Encodes `src` into `dst` using libosmocore's Base64 buffer contract.
///
/// On a non-empty successful input, writes the encoded bytes, a trailing NUL,
/// and sets `olen` to the encoded length excluding that NUL. If `dst` is too
/// short, it writes nothing, sets `olen` to the required capacity including
/// the NUL, and returns [`BASE64_BUFFER_TOO_SMALL`]. Empty input sets `olen`
/// to zero and leaves `dst` unchanged.
pub fn osmo_base64_encode(dst: &mut [u8], olen: &mut usize, src: &[u8]) -> i32 {
    if src.is_empty() {
        *olen = 0;
        return 0;
    }

    let encoded_len = ((src.len() + 2) / 3) * 4;
    if dst.len() < encoded_len + 1 {
        *olen = encoded_len + 1;
        return BASE64_BUFFER_TOO_SMALL;
    }

    let mut input_index = 0;
    let mut output_index = 0;
    while input_index + 3 <= src.len() {
        let first = src[input_index];
        let second = src[input_index + 1];
        let third = src[input_index + 2];
        dst[output_index] = ENCODE_MAP[(first >> 2) as usize];
        dst[output_index + 1] = ENCODE_MAP[(((first & 3) << 4) | (second >> 4)) as usize];
        dst[output_index + 2] = ENCODE_MAP[(((second & 15) << 2) | (third >> 6)) as usize];
        dst[output_index + 3] = ENCODE_MAP[(third & 63) as usize];
        input_index += 3;
        output_index += 4;
    }

    if input_index < src.len() {
        let first = src[input_index];
        let second = src.get(input_index + 1).copied().unwrap_or(0);
        dst[output_index] = ENCODE_MAP[(first >> 2) as usize];
        dst[output_index + 1] = ENCODE_MAP[(((first & 3) << 4) | (second >> 4)) as usize];
        dst[output_index + 2] = if input_index + 1 < src.len() {
            ENCODE_MAP[((second & 15) << 2) as usize]
        } else {
            b'='
        };
        dst[output_index + 3] = b'=';
        output_index += 4;
    }

    *olen = output_index;
    dst[output_index] = 0;
    0
}

/// Decodes `src` using libosmocore's Base64 buffer contract.
///
/// `None` performs a size probe. As in C, invalid input and input containing
/// only ignorable whitespace return without changing `olen`; callers who need
/// to observe that distinction should initialize it before calling.
pub fn osmo_base64_decode(dst: Option<&mut [u8]>, olen: &mut usize, src: &[u8]) -> i32 {
    let mut input_index = 0;
    let mut character_count = 0usize;
    let mut padding_count = 0usize;

    while input_index < src.len() {
        let mut spaces = 0;
        while input_index < src.len() && src[input_index] == b' ' {
            input_index += 1;
            spaces += 1;
        }
        if input_index == src.len() {
            break;
        }
        if input_index + 1 < src.len() && src[input_index] == b'\r' && src[input_index + 1] == b'\n'
        {
            input_index += 2;
            continue;
        }
        if src[input_index] == b'\n' {
            input_index += 1;
            continue;
        }
        if spaces != 0 {
            return BASE64_INVALID_INPUT;
        }

        let value = match decode_value(src[input_index]) {
            Some(value) => value,
            None => return BASE64_INVALID_INPUT,
        };
        if value == 64 {
            padding_count += 1;
            if padding_count > 2 {
                return BASE64_INVALID_INPUT;
            }
        } else if padding_count != 0 {
            return BASE64_INVALID_INPUT;
        }
        character_count += 1;
        input_index += 1;
    }

    if character_count == 0 {
        return 0;
    }

    let required = ((character_count * 6 + 7) >> 3) - padding_count;
    let dst = match dst {
        Some(dst) if dst.len() >= required => dst,
        _ => {
            *olen = required;
            return BASE64_BUFFER_TOO_SMALL;
        }
    };

    let mut remaining_output_bytes = 3usize;
    let mut quartet_len = 0usize;
    let mut accumulator = 0u32;
    let mut output_index = 0usize;
    for &byte in src {
        if matches!(byte, b'\r' | b'\n' | b' ') {
            continue;
        }
        let value = decode_value(byte).expect("input was validated");
        remaining_output_bytes -= usize::from(value == 64);
        accumulator = accumulator.wrapping_shl(6) | u32::from(value & 63);
        quartet_len += 1;
        if quartet_len == 4 {
            quartet_len = 0;
            if remaining_output_bytes > 0 {
                dst[output_index] = (accumulator >> 16) as u8;
                output_index += 1;
            }
            if remaining_output_bytes > 1 {
                dst[output_index] = (accumulator >> 8) as u8;
                output_index += 1;
            }
            if remaining_output_bytes > 2 {
                dst[output_index] = accumulator as u8;
                output_index += 1;
            }
        }
    }

    *olen = output_index;
    0
}

fn decode_value(byte: u8) -> Option<u8> {
    match byte {
        b'A'..=b'Z' => Some(byte - b'A'),
        b'a'..=b'z' => Some(byte - b'a' + 26),
        b'0'..=b'9' => Some(byte - b'0' + 52),
        b'+' => Some(62),
        b'/' => Some(63),
        b'=' => Some(64),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trip_valid_byte_strings() {
        for input in [b"f".as_slice(), b"fo", b"foo", b"foobar", &[0, 255, 1, 2]] {
            let mut encoded = vec![0; ((input.len() + 2) / 3) * 4 + 1];
            let mut encoded_len = usize::MAX;
            assert_eq!(osmo_base64_encode(&mut encoded, &mut encoded_len, input), 0);
            let mut decoded = vec![0; input.len()];
            let mut decoded_len = usize::MAX;
            assert_eq!(
                osmo_base64_decode(
                    Some(&mut decoded),
                    &mut decoded_len,
                    &encoded[..encoded_len]
                ),
                0
            );
            assert_eq!(&decoded[..decoded_len], input);
        }
    }

    #[test]
    fn empty_decode_preserves_olen_as_in_c() {
        let mut output = [];
        let mut olen = 7;
        assert_eq!(osmo_base64_decode(Some(&mut output), &mut olen, b""), 0);
        assert_eq!(olen, 7);
    }

    #[test]
    fn too_small_buffers_report_required_capacity() {
        let mut output = [0xa5; 4];
        let mut olen = 7;
        assert_eq!(
            osmo_base64_encode(&mut output, &mut olen, b"foo"),
            BASE64_BUFFER_TOO_SMALL
        );
        assert_eq!(olen, 5);
        assert_eq!(output, [0xa5; 4]);

        let mut olen = 7;
        assert_eq!(
            osmo_base64_decode(None, &mut olen, b"TWE="),
            BASE64_BUFFER_TOO_SMALL
        );
        assert_eq!(olen, 2);
    }

    #[test]
    fn invalid_input_does_not_change_olen() {
        let mut output = [0xa5; 2];
        let mut olen = 7;
        assert_eq!(
            osmo_base64_decode(Some(&mut output), &mut olen, b"TW$="),
            BASE64_INVALID_INPUT
        );
        assert_eq!(olen, 7);
        assert_eq!(output, [0xa5; 2]);
    }
}
