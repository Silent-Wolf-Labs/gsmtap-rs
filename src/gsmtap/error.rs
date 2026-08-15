use std::{error::Error, fmt};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ParseError {
    Truncated { needed: usize, actual: usize },
    InvalidHeaderLength { words: u8 },
    UnsupportedVersion { version: u8 },
}
impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Truncated { needed, actual } => write!(
                f,
                "truncated GSMTAP packet: need {needed} bytes, got {actual}"
            ),
            Self::InvalidHeaderLength { words } => {
                write!(f, "invalid GSMTAP header length: {words} words")
            }
            Self::UnsupportedVersion { version } => {
                write!(f, "unsupported GSMTAP version: {version}")
            }
        }
    }
}
impl Error for ParseError {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EncodeError {
    InvalidHeaderLength { declared: usize, expected: usize },
    InvalidExtensionLength { length: usize },
}
impl fmt::Display for EncodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidHeaderLength { declared, expected } => write!(
                f,
                "invalid GSMTAP header length: declared {declared} bytes, expected {expected}"
            ),
            Self::InvalidExtensionLength { length } => {
                write!(f, "invalid GSMTAP extension length: {length} bytes")
            }
        }
    }
}
impl Error for EncodeError {}
