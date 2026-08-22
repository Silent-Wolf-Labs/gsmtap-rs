use std::sync::Arc;

use gsmtap_rs::workbench::{config::Config, history::PacketStore};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let config = Config::from_args().map_err(std::io::Error::other)?;
    let store = Arc::new(PacketStore::new(config.history_capacity));
    gsmtap_rs::workbench::network::run(config, store).await
}
