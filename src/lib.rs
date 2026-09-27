//! GSMTAP parsing and encoding primitives.

pub mod gsmtap;
mod workbench;

/// Runs the internal workbench application used by the `gsmtap-workbench` binary.
#[doc(hidden)]
pub async fn run_workbench() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let config = workbench::config::Config::from_args().map_err(std::io::Error::other)?;
    let store = std::sync::Arc::new(workbench::history::PacketStore::new(
        config.history_capacity,
    ));
    workbench::network::run(config, store).await
}
