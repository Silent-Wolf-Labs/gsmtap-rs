use std::sync::Arc;

use axum::{routing::get, Router};
use tokio::net::UdpSocket;

use super::{
    api::{self, AppState},
    config::{Config, Mode},
    dto::{from_decoded, from_error},
    history::PacketStore,
};
use crate::gsmtap::parse;

pub fn build_router(config: Config, store: Arc<PacketStore>, sender: Arc<UdpSocket>) -> Router {
    let state = AppState {
        config,
        store,
        sender,
    };
    Router::new()
        .route("/", get(api::index))
        .route("/app.js", get(api::javascript))
        .route("/style.css", get(api::stylesheet))
        .route("/api/status", get(api::status))
        .route("/api/packets", get(api::packets))
        .route("/api/events", get(api::events))
        .route("/api/encode-send", axum::routing::post(api::encode_send))
        .route("/api/packets/:id/replay", axum::routing::post(api::replay))
        .route(
            "/api/packets/:id/modify-send",
            axum::routing::post(api::modify_send),
        )
        .with_state(state)
}

pub async fn run(
    config: Config,
    store: Arc<PacketStore>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let receiver = UdpSocket::bind(config.gsmtap_listen).await?;
    let sender = Arc::new(UdpSocket::bind("0.0.0.0:0").await?);
    let router = build_router(config.clone(), store.clone(), sender.clone());

    let http = tokio::spawn(async move {
        axum::Server::bind(&config.http_listen)
            .serve(router.into_make_service())
            .await
    });
    let rx = receive_loop(receiver, store, config.mode, config.gsmtap_forward, sender);
    tokio::select! {
        result = http => result??,
        result = rx => result?,
    }
    Ok(())
}

pub async fn receive_loop(
    socket: UdpSocket,
    store: Arc<PacketStore>,
    mode: Mode,
    forward: Option<std::net::SocketAddr>,
    sender: Arc<UdpSocket>,
) -> Result<(), std::io::Error> {
    let mut buffer = vec![0u8; 65535];
    loop {
        let (length, peer) = socket.recv_from(&mut buffer).await?;
        let bytes = &buffer[..length];
        let mut record = match parse(bytes) {
            Ok(packet) => from_decoded("RX", peer.to_string(), bytes, &packet),
            Err(error) => from_error("RX", peer.to_string(), bytes, error.to_string()),
        };
        record.mode = format!("{mode:?}").to_lowercase();
        record.destination = forward.map(|address| address.to_string());
        if mode == Mode::Relay {
            let destination = forward
                .ok_or_else(|| std::io::Error::other("relay forward endpoint is missing"))?;
            match sender.send_to(bytes, destination).await {
                Ok(_) => record.forward_status = Some("sent".into()),
                Err(error) => record.forward_status = Some(format!("error: {error}")),
            }
        }
        store.record(record).await;
    }
}
