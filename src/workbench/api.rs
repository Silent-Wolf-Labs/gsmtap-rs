use std::sync::Arc;

use axum::{
    extract::State,
    http::StatusCode,
    response::sse::{Event, KeepAlive, Sse},
    response::Html,
    Json,
};
use tokio_stream::{wrappers::BroadcastStream, StreamExt};

use crate::gsmtap::{GsmtapEncodeInput, GsmtapHeader};

use super::{
    config::Config,
    dto::{from_decoded, hex, parse_hex, EncodeSendRequest, PacketRecord, SendResponse},
    history::PacketStore,
};

#[derive(Clone)]
pub struct AppState {
    pub config: Config,
    pub store: Arc<PacketStore>,
    pub sender: Arc<tokio::net::UdpSocket>,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StatusResponse {
    pub gsmtap_listen: String,
    pub gsmtap_target: String,
    pub http_listen: String,
    pub receive_state: &'static str,
}

pub async fn status(State(state): State<AppState>) -> Json<StatusResponse> {
    Json(StatusResponse {
        gsmtap_listen: state.config.gsmtap_listen.to_string(),
        gsmtap_target: state.config.gsmtap_target.to_string(),
        http_listen: state.config.http_listen.to_string(),
        receive_state: "listening",
    })
}

pub async fn index() -> Html<&'static str> {
    Html(include_str!("../../static/index.html"))
}

pub async fn javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/app.js"),
    )
}

pub async fn stylesheet() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "text/css")],
        include_str!("../../static/style.css"),
    )
}

pub async fn packets(State(state): State<AppState>) -> Json<Vec<PacketRecord>> {
    Json(state.store.list().await)
}

pub async fn encode_send(
    State(state): State<AppState>,
    Json(request): Json<EncodeSendRequest>,
) -> Result<Json<SendResponse>, (StatusCode, String)> {
    let extension = parse_hex(&request.extension_hex).map_err(bad_request)?;
    let payload = parse_hex(&request.payload_hex).map_err(bad_request)?;
    let header = GsmtapHeader::new(
        request.version,
        request.header_length_words,
        request.message_type,
        request.timeslot,
        request.arfcn,
        request.signal_dbm,
        request.snr_db,
        request.frame_number,
        request.subtype,
        request.antenna_number,
        request.sub_slot,
        request.reserved,
    );
    let input = GsmtapEncodeInput::new(header, &extension, &payload)
        .map_err(|error| bad_request(error.to_string()))?;
    let encoded = input.encode();
    state
        .sender
        .send_to(&encoded, state.config.gsmtap_target)
        .await
        .map_err(|error| (StatusCode::BAD_GATEWAY, format!("UDP send failed: {error}")))?;
    let decoded = crate::gsmtap::parse(&encoded)
        .map_err(|error| (StatusCode::INTERNAL_SERVER_ERROR, error.to_string()))?;
    state
        .store
        .record(from_decoded(
            "TX",
            state.config.gsmtap_target.to_string(),
            &encoded,
            &decoded,
        ))
        .await;
    Ok(Json(SendResponse {
        timestamp_ms: std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis(),
        destination: state.config.gsmtap_target.to_string(),
        encoded_hex: hex(&encoded),
    }))
}

fn bad_request(error: impl ToString) -> (StatusCode, String) {
    (StatusCode::BAD_REQUEST, error.to_string())
}

pub async fn events(
    State(state): State<AppState>,
) -> Sse<impl tokio_stream::Stream<Item = Result<Event, axum::Error>>> {
    let stream = BroadcastStream::new(state.store.subscribe())
        .filter_map(|packet| packet.ok())
        .map(|packet| {
            Ok(Event::default()
                .json_data(packet)
                .expect("packet DTO is serializable"))
        });
    Sse::new(stream).keep_alive(KeepAlive::default())
}
