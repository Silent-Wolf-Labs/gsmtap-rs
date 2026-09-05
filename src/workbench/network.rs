use std::{
    net::SocketAddr,
    sync::Arc,
    time::{Duration, Instant},
};

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

const MAX_UDP_DATAGRAM_SIZE: usize = 65_535;
#[cfg(test)]
const DEFAULT_INGRESS_CAPACITY: usize = 1_024;
const FORWARD_RESOLUTION_REFRESH_INTERVAL: Duration = Duration::from_secs(60);
const FORWARD_RESOLUTION_RETRY_INTERVAL: Duration = Duration::from_secs(5);

pub fn build_router(config: Config, store: Arc<PacketStore>, sender: Arc<UdpSocket>) -> Router {
    let state = AppState {
        config,
        store,
        sender,
    };
    Router::new()
        .route("/", get(api::index))
        .route("/app.js", get(api::javascript))
        .route(
            "/controllers/app-controller.js",
            get(api::app_controller_javascript),
        )
        .route(
            "/components/panels/mode-panel.js",
            get(api::mode_panel_javascript),
        )
        .route(
            "/components/panels/listen-panel.js",
            get(api::listen_panel_javascript),
        )
        .route(
            "/components/panels/relay-panel.js",
            get(api::relay_panel_javascript),
        )
        .route(
            "/components/packet/packet-schema.js",
            get(api::packet_schema_javascript),
        )
        .route("/components/tooltip.js", get(api::tooltip_javascript))
        .route("/styles/style.css", get(api::stylesheet))
        .route(
            "/components/packet/packet-table.js",
            get(api::packet_table_javascript),
        )
        .route("/components/filters.js", get(api::filters_javascript))
        .route(
            "/components/select-control.js",
            get(api::select_control_javascript),
        )
        .route("/services/api.js", get(api::api_javascript))
        .route("/services/events.js", get(api::events_javascript))
        .route("/components/cards/card.js", get(api::card_javascript))
        .route(
            "/components/cards/status-card.js",
            get(api::status_card_javascript),
        )
        .route(
            "/components/panels/modify-panel.js",
            get(api::modify_panel_javascript),
        )
        .route("/models/modify-model.js", get(api::modify_model_javascript))
        .route(
            "/components/modify/modify-fields.js",
            get(api::modify_fields_javascript),
        )
        .route(
            "/components/modify/modify-form.js",
            get(api::modify_form_javascript),
        )
        .route(
            "/components/modify/modify-preview.js",
            get(api::modify_preview_javascript),
        )
        .route(
            "/components/modify/modify-state.js",
            get(api::modify_state_javascript),
        )
        .route(
            "/components/modify/modify-validation.js",
            get(api::modify_validation_javascript),
        )
        .route(
            "/services/modification-service.js",
            get(api::modification_service_javascript),
        )
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
    let http_listen = config.http_listen;

    let http = async move {
        axum::Server::bind(&http_listen)
            .serve(router.into_make_service())
            .await
    };
    let (ingress_tx, ingress_rx) = mpsc::channel(config.ingress_capacity);
    let intake = udp_receiver(receiver, ingress_tx, store.clone());
    let forward = ForwardTarget::new(config.gsmtap_forward).await;
    let processing = inspection_worker(ingress_rx, store, config.mode, forward, sender);
    tokio::select! {
        result = http => result?,
        result = intake => result?,
        result = processing => result?,
    }
    Ok(())
}

/// Runs the canonical UDP intake and inspection pipeline without the HTTP server.
///
/// This is retained for embedding and integration tests; unlike the historical
/// implementation, it shares the same bounded ingress queue and worker as `run`.
#[doc(hidden)]
#[cfg(test)]
pub async fn receive_loop(
    socket: UdpSocket,
    store: Arc<PacketStore>,
    mode: Mode,
    forward: Option<String>,
    sender: Arc<UdpSocket>,
) -> Result<(), std::io::Error> {
    let (ingress_tx, ingress_rx) = mpsc::channel(DEFAULT_INGRESS_CAPACITY);
    let target = ForwardTarget::new(forward).await;
    tokio::select! {
        result = udp_receiver(socket, ingress_tx, store.clone()) => result,
        result = inspection_worker(ingress_rx, store, mode, target, sender) => result,
    }
}

struct ReceivedDatagram {
    bytes: Vec<u8>,
    peer: SocketAddr,
    timestamp_ms: u128,
}

async fn udp_receiver(
    socket: UdpSocket,
    ingress: mpsc::Sender<ReceivedDatagram>,
    store: Arc<PacketStore>,
) -> Result<(), std::io::Error> {
    let mut buffer = vec![0u8; MAX_UDP_DATAGRAM_SIZE];
    loop {
        let (length, peer) = socket.recv_from(&mut buffer).await?;
        store.counters().received();
        let datagram = ReceivedDatagram {
            bytes: buffer[..length].to_vec(),
            peer,
            timestamp_ms: now_ms(),
        };
        match ingress.try_send(datagram) {
            Ok(()) => {}
            Err(mpsc::error::TrySendError::Full(_)) => store.counters().ingress_dropped(),
            Err(mpsc::error::TrySendError::Closed(_)) => return Ok(()),
        }
    }
}

async fn inspection_worker(
    mut ingress: mpsc::Receiver<ReceivedDatagram>,
    store: Arc<PacketStore>,
    mode: Mode,
    mut forward: ForwardTarget,
    sender: Arc<UdpSocket>,
) -> Result<(), std::io::Error> {
    while let Some(datagram) = ingress.recv().await {
        inspect_datagram(datagram, &store, mode, &mut forward, &sender).await;
    }
    Ok(())
}

async fn inspect_datagram(
    datagram: ReceivedDatagram,
    store: &PacketStore,
    mode: Mode,
    forward: &mut ForwardTarget,
    sender: &UdpSocket,
) {
    let bytes = &datagram.bytes;
    let mut record = match parse(bytes) {
        Ok(packet) => from_decoded("RX", mode, datagram.peer.to_string(), bytes, &packet),
        Err(error) => {
            store.counters().parse_failed();
            from_error(
                "RX",
                mode,
                datagram.peer.to_string(),
                bytes,
                error.to_string(),
            )
        }
    };
    record.timestamp_ms = datagram.timestamp_ms;
    record.destination = forward.configured.clone();
    if mode == Mode::Relay {
        record.forward_status = Some(match forward.send(sender, bytes).await {
            Ok(()) => {
                store.counters().forward_sent();
                "sent".into()
            }
            Err(error) => {
                store.counters().forward_failed();
                format!("error: {error}")
            }
        });
    }
    store.record(record).await;
}

struct ForwardTarget {
    configured: Option<String>,
    address: Option<SocketAddr>,
    last_error: Option<String>,
    next_resolution_attempt: Instant,
}

impl ForwardTarget {
    async fn new(configured: Option<String>) -> Self {
        let mut target = Self {
            configured,
            address: None,
            last_error: None,
            next_resolution_attempt: Instant::now(),
        };
        target.refresh().await;
        target
    }

    async fn send(&mut self, sender: &UdpSocket, bytes: &[u8]) -> Result<(), String> {
        if self.configured.is_none() {
            return Err("relay forward endpoint is missing".into());
        }
        if self.address.is_none() || Instant::now() >= self.next_resolution_attempt {
            self.refresh().await;
        }
        let address = self.address.ok_or_else(|| {
            self.last_error
                .clone()
                .unwrap_or_else(|| "destination resolved to no addresses".into())
        })?;
        if let Err(error) = sender.send_to(bytes, address).await {
            let message = format!("UDP send to {address} failed: {error}");
            self.address = None;
            self.refresh().await;
            return Err(message);
        }
        Ok(())
    }

    async fn refresh(&mut self) {
        let Some(destination) = self.configured.as_deref() else {
            return;
        };
        match tokio::net::lookup_host(destination).await {
            Ok(mut addresses) => match addresses.find(SocketAddr::is_ipv4) {
                Some(address) => {
                    self.address = Some(address);
                    self.last_error = None;
                    self.next_resolution_attempt =
                        Instant::now() + FORWARD_RESOLUTION_REFRESH_INTERVAL;
                }
                None => {
                    self.address = None;
                    self.last_error = Some("destination resolved to no IPv4 addresses".into());
                    self.next_resolution_attempt =
                        Instant::now() + FORWARD_RESOLUTION_RETRY_INTERVAL;
                }
            },
            Err(error) => {
                self.address = None;
                self.last_error = Some(format!("destination lookup failed: {error}"));
                self.next_resolution_attempt = Instant::now() + FORWARD_RESOLUTION_RETRY_INTERVAL;
            }
        }
    }
}

fn now_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
}
