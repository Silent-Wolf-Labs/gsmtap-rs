//! Typed access to fixture records; a missing or malformed field is an error.

use serde_json::{Map, Value};

pub(crate) struct Record<'a>(pub(crate) &'a Map<String, Value>);

impl<'a> Record<'a> {
    pub(crate) fn value(&self, key: &str) -> Result<&'a Value, String> {
        self.0
            .get(key)
            .ok_or_else(|| format!("record has no `{key}`"))
    }

    pub(crate) fn str(&self, key: &str) -> Result<&'a str, String> {
        self.value(key)?
            .as_str()
            .ok_or_else(|| format!("`{key}` is not a string"))
    }

    pub(crate) fn int(&self, key: &str) -> Result<i64, String> {
        self.value(key)?
            .as_i64()
            .ok_or_else(|| format!("`{key}` is not an integer"))
    }

    pub(crate) fn uint(&self, key: &str) -> Result<u64, String> {
        self.value(key)?
            .as_u64()
            .ok_or_else(|| format!("`{key}` is not an unsigned integer"))
    }

    pub(crate) fn usize(&self, key: &str) -> Result<usize, String> {
        usize::try_from(self.uint(key)?).map_err(|_| format!("`{key}` does not fit usize"))
    }

    pub(crate) fn i32(&self, key: &str) -> Result<i32, String> {
        i32::try_from(self.int(key)?).map_err(|_| format!("`{key}` does not fit a C int"))
    }

    pub(crate) fn bool(&self, key: &str) -> Result<bool, String> {
        self.value(key)?
            .as_bool()
            .ok_or_else(|| format!("`{key}` is not a boolean"))
    }

    pub(crate) fn hex(&self, key: &str) -> Result<Vec<u8>, String> {
        parse_hex(self.str(key)?).ok_or_else(|| format!("`{key}` is not hex"))
    }

    /// Parses a `;`-separated integer list; an empty string is an empty list.
    pub(crate) fn list(&self, key: &str) -> Result<Vec<i64>, String> {
        let text = self.str(key)?;
        if text.is_empty() {
            return Ok(Vec::new());
        }
        text.split(';')
            .map(|item| item.parse().map_err(|_| format!("`{key}` is not a list")))
            .collect()
    }
}

/// Parses a JSON array fixture of objects.
pub(crate) fn array(text: &str) -> Result<Vec<Map<String, Value>>, String> {
    let values: Vec<Value> =
        serde_json::from_str(text).map_err(|error| format!("invalid JSON: {error}"))?;
    values.into_iter().map(object).collect()
}

/// Parses one object per non-empty line.
pub(crate) fn lines(text: &str) -> Result<Vec<Map<String, Value>>, String> {
    text.lines()
        .enumerate()
        .filter(|(_, line)| !line.is_empty())
        .map(|(index, line)| {
            serde_json::from_str(line)
                .map_err(|error| format!("line {}: invalid JSON: {error}", index + 1))
                .and_then(object)
        })
        .collect()
}

pub(crate) fn object(value: Value) -> Result<Map<String, Value>, String> {
    match value {
        Value::Object(map) => Ok(map),
        _ => Err("record is not an object".to_owned()),
    }
}

pub(crate) fn parse_hex(text: &str) -> Option<Vec<u8>> {
    if !text.len().is_multiple_of(2) {
        return None;
    }
    (0..text.len())
        .step_by(2)
        .map(|index| u8::from_str_radix(text.get(index..index + 2)?, 16).ok())
        .collect()
}

pub(crate) fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|byte| format!("{byte:02x}")).collect()
}
