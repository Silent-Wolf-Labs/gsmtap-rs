//! GSMTAP parsing and encoding primitives.

pub mod gsmtap;
mod workbench;

/// Runs the internal workbench application used by the `gsmtap-workbench` binary.
#[doc(hidden)]
pub async fn run_workbench() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let config = workbench::config::Config::from_args().map_err(std::io::Error::other)?;
    let references = match &config.reference_vectors {
        Some(directory) => workbench::reference::ReferenceVectors::load(directory)
            .map_err(std::io::Error::other)?,
        None => Default::default(),
    };
    let store = std::sync::Arc::new(workbench::history::PacketStore::with_references(
        config.history_capacity,
        references,
    ));
    workbench::network::run(config, store).await
}
