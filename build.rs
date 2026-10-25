//! Records the locked osmocore-rs version so conformance reports name what they verified.

use std::path::Path;

fn main() {
    let lock = Path::new(&std::env::var("CARGO_MANIFEST_DIR").unwrap()).join("Cargo.lock");
    println!("cargo:rerun-if-changed={}", lock.display());
    let version = std::fs::read_to_string(&lock)
        .ok()
        .and_then(|text| osmocore_version(&text))
        .unwrap_or_else(|| "unknown".to_owned());
    println!("cargo:rustc-env=GSMTAP_OSMOCORE_RS_VERSION={version}");
}

/// Returns `<version> (<source>)`, where the source is a registry or `path`.
fn osmocore_version(lock: &str) -> Option<String> {
    let package = lock
        .split("[[package]]")
        .find(|package| package.lines().any(|line| line == "name = \"osmocore-rs\""))?;
    let field = |key: &str| {
        package.lines().find_map(|line| {
            line.strip_prefix(key)?
                .strip_prefix(" = \"")?
                .strip_suffix('"')
        })
    };
    let source = match field("source") {
        Some(source) if source.contains("crates.io") => "crates.io",
        Some(source) => source,
        None => "path",
    };
    Some(format!("{} ({source})", field("version")?))
}
