//! Independent Rust replacements for selected `libosmocore` APIs.
//!
//! Conversion surfaces include GSMTAP packet construction, Base64 buffers,
//! hexadecimal parsing, BCD encoding and decoding, and basic/extended packed
//! bit conversion.

pub mod base64;
pub mod bcd;
pub mod bits;
pub mod gsmtap;
pub mod hexparse;
