use std::sync::Arc;

use axum::{routing::get, Router};
use tokio::net::UdpSocket;
use tokio::sync::mpsc;

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
        .route(
            "/api/packets/:id/modify-preview",
            axum::routing::post(api::modify_preview),
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
    let ingress_capacity = config.history_capacity.max(1024);
    let (ingress_tx, ingress_rx) = mpsc::channel(ingress_capacity);
    let intake = tokio::spawn(udp_receiver(receiver, ingress_tx, store.clone()));
    let processing = inspection_worker(
        ingress_rx,
        store,
        config.mode,
        config.gsmtap_forward,
        sender,
    );
    tokio::select! {
        result = http => result??,
        result = intake => result??,
        result = processing => result?,
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
        store.counters().received();
        inspect_datagram(
            ReceivedDatagram {
                bytes: buffer[..length].to_vec(),
                peer,
                timestamp_ms: now_ms(),
            },
            &store,
            mode,
            forward,
            &sender,
        )
        .await;
    }
}

struct ReceivedDatagram {
    bytes: Vec<u8>,
    peer: std::net::SocketAddr,
    timestamp_ms: u128,
}

async fn udp_receiver(
    socket: UdpSocket,
    ingress: mpsc::Sender<ReceivedDatagram>,
    store: Arc<PacketStore>,
) -> Result<(), std::io::Error> {
    let mut buffer = vec![0u8; 65535];
    loop {
        let (length, peer) = socket.recv_from(&mut buffer).await?;
        store.counters().received();
        let datagram = ReceivedDatagram {
            bytes: buffer[..length].to_vec(),
            peer,
            timestamp_ms: now_ms(),
        };
        if ingress.try_send(datagram).is_err() {
            store.counters().ingress_dropped();
        }
    }
}

async fn inspection_worker(
    mut ingress: mpsc::Receiver<ReceivedDatagram>,
    store: Arc<PacketStore>,
    mode: Mode,
    forward: Option<std::net::SocketAddr>,
    sender: Arc<UdpSocket>,
) -> Result<(), std::io::Error> {
    while let Some(datagram) = ingress.recv().await {
        inspect_datagram(datagram, &store, mode, forward, &sender).await;
    }
    Ok(())
}

async fn inspect_datagram(
    datagram: ReceivedDatagram,
    store: &PacketStore,
    mode: Mode,
    forward: Option<std::net::SocketAddr>,
    sender: &UdpSocket,
) {
    let bytes = &datagram.bytes;
    let mut record = match parse(bytes) {
        Ok(packet) => from_decoded("RX", datagram.peer.to_string(), bytes, &packet),
        Err(error) => {
            store.counters().parse_failed();
            from_error("RX", datagram.peer.to_string(), bytes, error.to_string())
        }
    };
    record.timestamp_ms = datagram.timestamp_ms;
    record.mode = format!("{mode:?}").to_lowercase();
    record.destination = forward.map(|address| address.to_string());
    if mode == Mode::Relay {
        if let Some(destination) = forward {
            match sender.send_to(bytes, destination).await {
                Ok(_) => record.forward_status = Some("sent".into()),
                Err(error) => record.forward_status = Some(format!("error: {error}")),
            }
        } else {
            record.forward_status = Some("error: relay forward endpoint is missing".into());
        }
    }
    store.record(record).await;
}

fn now_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
}
