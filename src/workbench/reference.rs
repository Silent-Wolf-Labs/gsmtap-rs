//! C-recorded GSMTAP packets that received traffic is matched against.

use std::collections::HashMap;
use std::path::Path;

use super::dto::parse_hex;

#[derive(serde::Deserialize)]
struct Vector {
    case: String,
    encoded_hex: String,
}

/// Recorded packets keyed by their exact bytes.
#[derive(Debug, Default)]
pub struct ReferenceVectors {
    cases: HashMap<Vec<u8>, String>,
}

impl ReferenceVectors {
    /// Loads every `*.json` vector directly inside `directory`, in file-name
    /// order. When two vectors encode the same bytes, the first case wins.
    pub fn load(directory: &Path) -> Result<Self, String> {
        let entries = std::fs::read_dir(directory)
            .map_err(|error| format!("cannot read {}: {error}", directory.display()))?;
        let mut paths = Vec::new();
        for entry in entries {
            let path = entry
                .map_err(|error| format!("cannot read {}: {error}", directory.display()))?
                .path();
            if path.is_file()
                && path
                    .extension()
                    .is_some_and(|extension| extension == "json")
            {
                paths.push(path);
            }
        }
        paths.sort();
        let mut vectors = Self::default();
        for path in &paths {
            let text = std::fs::read_to_string(path)
                .map_err(|error| format!("cannot read {}: {error}", path.display()))?;
            let vector: Vector = serde_json::from_str(&text)
                .map_err(|error| format!("invalid reference vector {}: {error}", path.display()))?;
            let bytes = parse_hex(&vector.encoded_hex)
                .map_err(|error| format!("invalid reference vector {}: {error}", path.display()))?;
            vectors.cases.entry(bytes).or_insert(vector.case);
        }
        if vectors.is_empty() {
            return Err(format!("no reference vectors in {}", directory.display()));
        }
        Ok(vectors)
    }

    /// The recorded case whose packet is exactly `bytes`.
    pub fn case(&self, bytes: &[u8]) -> Option<&str> {
        self.cases.get(bytes).map(String::as_str)
    }

    pub fn len(&self) -> usize {
        self.cases.len()
    }

    pub fn is_empty(&self) -> bool {
        self.cases.is_empty()
    }
}
