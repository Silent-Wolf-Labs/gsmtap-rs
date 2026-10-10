//! Independent Rust replacements for selected `libosmocore` APIs.
//!
//! Conversion surfaces include GSMTAP packet construction, Base64 buffers,
//! hexadecimal parsing, BCD encoding and decoding, and basic/extended packed
//! bit conversion, cursor-based bit-vector primitives, owned message-buffer
//! primitives, inter-layer primitive metadata/event mapping, and core CRCs.

pub mod core;
pub mod gsmtap;

// Preserve the original public module paths for existing consumers.
pub use crate::core::{base64, bcd, bits, hexparse};
