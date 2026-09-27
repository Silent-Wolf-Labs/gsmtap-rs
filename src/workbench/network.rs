use std::{net::SocketAddr, sync::Arc};

use axum::{routing::get, Router};
use tokio::net::UdpSocket;
use tokio::sync::{broadcast, mpsc};

use super::{
    api::{self, AppState},
    capture::CaptureControl,
    config::{Config, Mode},
    dto::{from_decoded, from_error},
    history::PacketStore,
    runtime::RuntimeHandle,
};
use crate::gsmtap::parse;

const MAX_UDP_DATAGRAM_SIZE: usize = 65_535;
#[cfg(test)]
const DEFAULT_INGRESS_CAPACITY: usize = 1_024;

#[cfg(test)]
pub fn build_router(config: Config, store: Arc<PacketStore>, sender: Arc<UdpSocket>) -> Router {
    build_router_with_capture(config, store, sender, CaptureControl::default())
}

#[cfg(test)]
pub fn build_router_with_capture(
    config: Config,
    store: Arc<PacketStore>,
    sender: Arc<UdpSocket>,
    capture: CaptureControl,
) -> Router {
    let runtime = RuntimeHandle::detached(config.clone(), store.clone(), capture.clone());
    build_router_with_runtime(config, store, sender, capture, runtime)
}

pub fn build_router_with_runtime(
    config: Config,
    store: Arc<PacketStore>,
    sender: Arc<UdpSocket>,
    capture: CaptureControl,
    runtime: RuntimeHandle,
) -> Router {
    let state = AppState {
        config,
        store,
        sender,
        capture,
        runtime,
    };
    Router::new()
        .route("/", get(api::index))
        .route("/app.js", get(api::javascript))
        .route(
            "/controllers/app-controller.js",
            get(api::app_controller_javascript),
        )
        .route(
            "/controllers/packet-history-controller.js",
            get(api::packet_history_controller_javascript),
        )
        .route(
            "/components/packet/packet-history-view.js",
            get(api::packet_history_view_javascript),
        )
        .route(
            "/components/dialogs/clear-history-dialog.js",
            get(api::clear_history_dialog_javascript),
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
        .route("/models/packet-model.js", get(api::packet_model_javascript))
        .route("/components/tooltip.js", get(api::tooltip_javascript))
        .route("/styles/style.css", get(api::stylesheet))
        .route("/styles/base.css", get(api::base_stylesheet))
        .route("/styles/forms.css", get(api::forms_stylesheet))
        .route("/styles/status-card.css", get(api::status_card_stylesheet))
        .route("/styles/modify.css", get(api::modify_stylesheet))
        .route(
            "/styles/packet-table.css",
            get(api::packet_table_stylesheet),
        )
        .route("/styles/pause-icon-32x32.png", get(api::pause_icon))
        .route("/styles/play-icon-32x32.png", get(api::play_icon))
        .route(
            "/components/packet/packet-table.js",
            get(api::packet_table_javascript),
        )
        .route(
            "/components/packet/column-filter.js",
            get(api::column_filter_javascript),
        )
        .route(
            "/components/packet/select-filter.js",
            get(api::select_filter_javascript),
        )
        .route(
            "/components/packet/text-filter.js",
            get(api::text_filter_javascript),
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
        .route("/api/mode", axum::routing::put(api::change_mode))
        .route(
            "/api/listen",
            axum::routing::put(api::change_listen_address),
        )
        .route("/api/capture", axum::routing::put(api::set_capture))
        .route("/api/packets", get(api::packets).delete(api::clear_packets))
        .route(
            "/api/packets/forward",
            axum::routing::post(api::forward_packets),
        )
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
    let sender = Arc::new(UdpSocket::bind("0.0.0.0:0").await?);
    let capture = CaptureControl::default();
    let runtime = RuntimeHandle::start(config.clone(), store.clone(), capture.clone()).await?;
    let router = build_router_with_runtime(
        config.clone(),
        store.clone(),
        sender.clone(),
        capture.clone(),
        runtime,
    );
    let http_listen = config.http_listen;

    let http = async move {
        axum::Server::bind(&http_listen)
            .serve(router.into_make_service())
            .await
    };
    http.await?;
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
    receive_loop_with_capture(
        socket,
        store,
        mode,
        forward,
        sender,
        CaptureControl::default(),
    )
    .await
}

#[doc(hidden)]
#[cfg(test)]
pub async fn receive_loop_with_capture(
    socket: UdpSocket,
    store: Arc<PacketStore>,
    mode: Mode,
    forward: Option<String>,
    _sender: Arc<UdpSocket>,
    capture: CaptureControl,
) -> Result<(), std::io::Error> {
    let (ingress_tx, ingress_rx) = mpsc::channel(DEFAULT_INGRESS_CAPACITY);
    let (shutdown, _) = broadcast::channel(1);
    tokio::select! {
        result = udp_receiver_until(Arc::new(socket), ingress_tx, store.clone(), capture, shutdown.subscribe()) => result,
        result = inspection_worker_until(ingress_rx, store, mode, forward, None, shutdown.subscribe()) => result,
    }
}

#[doc(hidden)]
#[cfg(test)]
pub async fn receive_loop_gated(
    socket: UdpSocket,
    store: Arc<PacketStore>,
    mode: Mode,
    _sender: Arc<UdpSocket>,
    capture: CaptureControl,
    gate: Arc<tokio::sync::Notify>,
) -> Result<(), std::io::Error> {
    let (ingress_tx, ingress_rx) = mpsc::channel(DEFAULT_INGRESS_CAPACITY);
    let (shutdown, _) = broadcast::channel(1);
    tokio::select! {
        result = udp_receiver_until(Arc::new(socket), ingress_tx, store.clone(), capture, shutdown.subscribe()) => result,
        result = inspection_worker_until(ingress_rx, store, mode, None, Some(gate), shutdown.subscribe()) => result,
    }
}

#[doc(hidden)]
#[cfg(test)]
pub async fn receive_loop_gated_with_capacity(
    socket: UdpSocket,
    store: Arc<PacketStore>,
    mode: Mode,
    forward: Option<String>,
    capture: CaptureControl,
    gate: Arc<tokio::sync::Notify>,
    capacity: usize,
) -> Result<(), std::io::Error> {
    let (ingress_tx, ingress_rx) = mpsc::channel(capacity);
    let (shutdown, _) = broadcast::channel(1);
    tokio::select! {
        result = udp_receiver_until(Arc::new(socket), ingress_tx, store.clone(), capture, shutdown.subscribe()) => result,
        result = inspection_worker_until(ingress_rx, store, mode, forward, Some(gate), shutdown.subscribe()) => result,
    }
}

pub(crate) struct ReceivedDatagram {
    bytes: Vec<u8>,
    peer: SocketAddr,
    timestamp_ms: u128,
    capture_enabled: bool,
}

pub(crate) async fn udp_receiver_until(
    socket: Arc<UdpSocket>,
    ingress: mpsc::Sender<ReceivedDatagram>,
    store: Arc<PacketStore>,
    capture: CaptureControl,
    mut shutdown: broadcast::Receiver<()>,
) -> Result<(), std::io::Error> {
    let mut buffer = vec![0u8; MAX_UDP_DATAGRAM_SIZE];
    loop {
        let (length, peer) = tokio::select! {
            _ = shutdown.recv() => return Ok(()),
            result = socket.recv_from(&mut buffer) => result?,
        };
        store.counters().received();
        let capture_enabled = !capture.is_paused();
        if !capture_enabled {
            store.counters().capture_skipped();
        }
        let datagram = ReceivedDatagram {
            bytes: buffer[..length].to_vec(),
            peer,
            timestamp_ms: now_ms(),
            capture_enabled,
        };
        match ingress.try_send(datagram) {
            Ok(()) => {}
            Err(mpsc::error::TrySendError::Full(_)) => store.counters().ingress_dropped(),
            Err(mpsc::error::TrySendError::Closed(_)) => return Ok(()),
        }
    }
}

pub(crate) async fn inspection_worker_until(
    mut ingress: mpsc::Receiver<ReceivedDatagram>,
    store: Arc<PacketStore>,
    mode: Mode,
    destination: Option<String>,
    gate: Option<Arc<tokio::sync::Notify>>,
    mut shutdown: broadcast::Receiver<()>,
) -> Result<(), std::io::Error> {
    loop {
        let datagram = tokio::select! {
            _ = shutdown.recv() => return Ok(()),
            datagram = ingress.recv() => match datagram {
                Some(datagram) => datagram,
                None => return Ok(()),
            },
        };
        if let Some(gate) = &gate {
            gate.notified().await;
        }
        inspect_datagram(datagram, &store, mode, destination.as_deref()).await;
    }
}

async fn inspect_datagram(
    datagram: ReceivedDatagram,
    store: &PacketStore,
    mode: Mode,
    destination: Option<&str>,
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
    record.destination = destination.map(String::from);
    if datagram.capture_enabled {
        store.record(record).await;
    }
}

fn now_ms() -> u128 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis()
}
