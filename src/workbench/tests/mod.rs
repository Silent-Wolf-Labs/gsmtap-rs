use std::{sync::Arc, time::Duration};

use super::{
    config::{Config, Mode},
    history::PacketStore,
};
use tokio::net::UdpSocket;

async fn wait_for_records(store: &PacketStore, count: usize) {
    tokio::time::timeout(Duration::from_secs(1), async {
        while store.list().await.len() < count {
            tokio::task::yield_now().await;
        }
    })
    .await
    .expect("packet history did not reach the expected size");
}

fn config(mode: Mode, listen: std::net::SocketAddr, forward: Option<String>) -> Config {
    Config::from_values(mode, listen, forward, "127.0.0.1:0".parse().unwrap(), 8, 16).unwrap()
}

async fn socket() -> UdpSocket {
    UdpSocket::bind("127.0.0.1:0").await.unwrap()
}

fn store() -> Arc<PacketStore> {
    Arc::new(PacketStore::new(8))
}

mod api;
mod config;
mod forwarding;
mod history;
mod modes;
