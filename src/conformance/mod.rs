//! Replays C-recorded fixtures through osmocore-rs and reports every difference.
//!
//! Expected values come only from the fixtures in `tests/vectors/`, which the
//! direct-call harnesses in `reference/` record from libosmocore. Each record
//! becomes one case that either matches, mismatches with named fields, or is
//! unsupported with a reason. Unsupported cases are never counted as matches.

mod base64;
mod bcd;
mod bits;
mod conv;
mod crc;
mod gsmtap;
mod hexparse;
mod record;

use std::fmt;
use std::path::Path;

use serde::Serialize;
use sha2::{Digest, Sha256};

/// Version and source of the osmocore-rs build being verified, from `Cargo.lock`.
pub const OSMOCORE_RS_VERSION: &str = env!("GSMTAP_OSMOCORE_RS_VERSION");

/// Modules the engine can verify, in report order.
pub const MODULES: &[&str] = &["hexparse", "bcd", "bits", "base64", "crc", "conv", "gsmtap"];

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Report {
    pub osmocore_rs_version: &'static str,
    pub modules: Vec<ModuleReport>,
}

impl Report {
    pub fn mismatched(&self) -> usize {
        self.modules
            .iter()
            .map(|module| module.mismatched.len())
            .sum()
    }

    pub fn unsupported(&self) -> usize {
        self.modules
            .iter()
            .map(|module| module.unsupported.len())
            .sum()
    }

    pub fn errors(&self) -> usize {
        self.modules
            .iter()
            .filter(|module| module.error.is_some())
            .count()
    }

    /// True when every case matched and every fixture loaded.
    pub fn passed(&self) -> bool {
        self.mismatched() == 0 && self.errors() == 0 && self.unsupported() == 0
    }
}

/// Cases listed per module and outcome in the text summary; the JSON has all.
const SUMMARY_CASES: usize = 20;

impl fmt::Display for Report {
    /// A plain-text summary: per-module counts, then failing cases.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "osmocore-rs {}", self.osmocore_rs_version)?;
        writeln!(
            f,
            "{:<10} {:>8} {:>8} {:>10} {:>11}",
            "module", "cases", "matched", "mismatched", "unsupported"
        )?;
        for module in &self.modules {
            writeln!(
                f,
                "{:<10} {:>8} {:>8} {:>10} {:>11}",
                module.module,
                module.cases(),
                module.matched,
                module.mismatched.len(),
                module.unsupported.len()
            )?;
        }
        for module in &self.modules {
            if let Some(error) = &module.error {
                writeln!(f, "{}: error: {error}", module.module)?;
            }
            for mismatch in module.mismatched.iter().take(SUMMARY_CASES) {
                let fields: Vec<_> = mismatch
                    .fields
                    .iter()
                    .map(|diff| format!("{} c={} rust={}", diff.field, diff.c, diff.rust))
                    .collect();
                writeln!(
                    f,
                    "{}: mismatch {}: {}",
                    module.module,
                    mismatch.case,
                    fields.join("; ")
                )?;
            }
            for unsupported in module.unsupported.iter().take(SUMMARY_CASES) {
                writeln!(
                    f,
                    "{}: unsupported {}: {}",
                    module.module, unsupported.case, unsupported.reason
                )?;
            }
            let omitted = module.mismatched.len().saturating_sub(SUMMARY_CASES)
                + module.unsupported.len().saturating_sub(SUMMARY_CASES);
            if omitted > 0 {
                writeln!(
                    f,
                    "{}: {omitted} more failing cases in the JSON report",
                    module.module
                )?;
            }
        }
        writeln!(
            f,
            "result: {}",
            if self.passed() { "passed" } else { "failed" }
        )
    }
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ModuleReport {
    pub module: &'static str,
    pub fixtures: Vec<FixtureInfo>,
    pub matched: usize,
    pub mismatched: Vec<Mismatch>,
    pub unsupported: Vec<Unsupported>,
    /// A fixture that could not be read or parsed; no cases were replayed from it.
    pub error: Option<String>,
}

impl ModuleReport {
    pub fn cases(&self) -> usize {
        self.matched + self.mismatched.len() + self.unsupported.len()
    }
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FixtureInfo {
    /// Path relative to the vectors directory.
    pub path: String,
    pub sha256: String,
    /// libosmocore revision recorded by the harness, or `unknown`.
    pub libosmocore_revision: String,
}

#[derive(Debug, Clone, Serialize)]
pub struct Mismatch {
    pub case: String,
    pub fields: Vec<FieldDiff>,
}

#[derive(Debug, Clone, Serialize)]
pub struct Unsupported {
    pub case: String,
    pub reason: String,
}

/// One recorded output field that osmocore-rs did not reproduce.
#[derive(Debug, Clone, PartialEq, Eq, Serialize)]
pub struct FieldDiff {
    pub field: String,
    pub c: String,
    pub rust: String,
}

/// Verifies the selected modules, or all of them, against `vectors`.
pub fn run(vectors: &Path, modules: &[&str]) -> Report {
    let modules = MODULES
        .iter()
        .filter(|module| modules.is_empty() || modules.contains(module))
        .map(|&module| {
            let mut run = ModuleRun::new(module, vectors);
            let result = match module {
                "hexparse" => hexparse::verify(&mut run),
                "bcd" => bcd::verify(&mut run),
                "bits" => bits::verify(&mut run),
                "base64" => base64::verify(&mut run),
                "crc" => crc::verify(&mut run),
                "conv" => conv::verify(&mut run),
                "gsmtap" => gsmtap::verify(&mut run),
                _ => unreachable!("MODULES lists every verifier"),
            };
            run.finish(result.err())
        })
        .collect();
    Report {
        osmocore_rs_version: OSMOCORE_RS_VERSION,
        modules,
    }
}

/// Collects fixture identities and case outcomes for one module.
pub(crate) struct ModuleRun<'a> {
    report: ModuleReport,
    vectors: &'a Path,
}

impl<'a> ModuleRun<'a> {
    fn new(module: &'static str, vectors: &'a Path) -> Self {
        Self {
            report: ModuleReport {
                module,
                fixtures: Vec::new(),
                matched: 0,
                mismatched: Vec::new(),
                unsupported: Vec::new(),
                error: None,
            },
            vectors,
        }
    }

    /// Reads a fixture relative to the vectors directory and records its hash.
    fn fixture(&mut self, path: &str) -> Result<String, String> {
        let bytes = std::fs::read(self.vectors.join(path))
            .map_err(|error| format!("cannot read {path}: {error}"))?;
        let text = String::from_utf8(bytes).map_err(|_| format!("{path} is not UTF-8"))?;
        self.report.fixtures.push(FixtureInfo {
            path: path.to_owned(),
            sha256: format!("{:x}", Sha256::digest(text.as_bytes())),
            libosmocore_revision: "unknown".to_owned(),
        });
        Ok(text)
    }

    /// Sets the revision recorded inside the most recently read fixture.
    fn revision(&mut self, revision: &str) {
        if let Some(fixture) = self.report.fixtures.last_mut() {
            fixture.libosmocore_revision = revision.to_owned();
        }
    }

    fn record(&mut self, case: impl fmt::Display, outcome: Outcome) {
        match outcome {
            Outcome::Match => self.report.matched += 1,
            Outcome::Mismatch(fields) => self.report.mismatched.push(Mismatch {
                case: case.to_string(),
                fields,
            }),
            Outcome::Unsupported(reason) => self.report.unsupported.push(Unsupported {
                case: case.to_string(),
                reason,
            }),
        }
    }

    fn finish(mut self, error: Option<String>) -> ModuleReport {
        self.report.error = error;
        self.report
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) enum Outcome {
    Match,
    Mismatch(Vec<FieldDiff>),
    Unsupported(String),
}

impl From<Result<Check, String>> for Outcome {
    /// A malformed record is unsupported: it cannot be replayed as recorded.
    fn from(result: Result<Check, String>) -> Self {
        match result {
            Ok(check) if check.diffs.is_empty() => Outcome::Match,
            Ok(check) => Outcome::Mismatch(check.diffs),
            Err(reason) => Outcome::Unsupported(reason),
        }
    }
}

/// Compares recorded C fields with osmocore-rs results.
#[derive(Debug, Default)]
pub(crate) struct Check {
    diffs: Vec<FieldDiff>,
}

impl Check {
    pub(crate) fn field<T: PartialEq + fmt::Debug>(&mut self, field: &str, c: T, rust: T) {
        if c != rust {
            self.diffs.push(FieldDiff {
                field: field.to_owned(),
                c: format!("{c:?}"),
                rust: format!("{rust:?}"),
            });
        }
    }

    pub(crate) fn bytes(&mut self, field: &str, c: &[u8], rust: &[u8]) {
        if c != rust {
            self.diffs.push(FieldDiff {
                field: field.to_owned(),
                c: record::hex(c),
                rust: record::hex(rust),
            });
        }
    }

    /// Records a Rust result that has no C equivalent for this call.
    pub(crate) fn rust_only(&mut self, field: &str, c: impl fmt::Debug, rust: impl fmt::Debug) {
        self.diffs.push(FieldDiff {
            field: field.to_owned(),
            c: format!("{c:?}"),
            rust: format!("{rust:?}"),
        });
    }
}

#[cfg(test)]
mod tests;
