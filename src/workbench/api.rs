use std::sync::Arc;

use axum::{
    extract::{Path, State},
    http::StatusCode,
    response::sse::{Event, KeepAlive, Sse},
    response::Html,
    Json,
};
use tokio_stream::{wrappers::BroadcastStream, StreamExt};

use crate::gsmtap::{GsmtapEncodeInput, GsmtapHeader};

use super::{
    config::{Config, Mode},
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
    pub mode: Mode,
    pub gsmtap_forward: Option<String>,
    pub http_listen: String,
    pub receive_state: &'static str,
}

pub async fn status(State(state): State<AppState>) -> Json<StatusResponse> {
    Json(StatusResponse {
        gsmtap_listen: state.config.gsmtap_listen.to_string(),
        mode: state.config.mode,
        gsmtap_forward: state.config.gsmtap_forward.map(|addr| addr.to_string()),
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
    require_modify(&state)?;
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
    let destination = state
        .config
        .gsmtap_forward
        .ok_or_else(|| bad_request("a forward endpoint is required"))?;
    state
        .sender
        .send_to(&encoded, destination)
        .await
        .map_err(|error| (StatusCode::BAD_GATEWAY, format!("UDP send failed: {error}")))?;
    let decoded = crate::gsmtap::parse(&encoded)
        .map_err(|error| (StatusCode::INTERNAL_SERVER_ERROR, error.to_string()))?;
    state
        .store
        .record(from_decoded(
            "TX",
            destination.to_string(),
            &encoded,
            &decoded,
        ))
        .await;
    Ok(Json(SendResponse {
        timestamp_ms: std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis(),
        destination: destination.to_string(),
        encoded_hex: hex(&encoded),
    }))
}

pub async fn replay(
    State(state): State<AppState>,
    Path(id): Path<u64>,
) -> Result<Json<SendResponse>, (StatusCode, String)> {
    require_modify(&state)?;
    let packet = state
        .store
        .get(id)
        .await
        .ok_or((StatusCode::NOT_FOUND, "packet not found".into()))?;
    let bytes = parse_hex(&packet.raw_hex).map_err(bad_request)?;
    send_record(&state, bytes, Some(packet.raw_hex), false).await
}

pub async fn modify_send(
    State(state): State<AppState>,
    Path(id): Path<u64>,
    Json(request): Json<EncodeSendRequest>,
) -> Result<Json<SendResponse>, (StatusCode, String)> {
    require_modify(&state)?;
    let packet = state
        .store
        .get(id)
        .await
        .ok_or((StatusCode::NOT_FOUND, "packet not found".into()))?;
    let original = packet.raw_hex.clone();
    let bytes = encode_request(&request)?;
    send_record(&state, bytes, Some(original), true).await
}

fn encode_request(request: &EncodeSendRequest) -> Result<Vec<u8>, (StatusCode, String)> {
    let extension = parse_hex(&request.extension_hex).map_err(bad_request)?;
    let payload = parse_hex(&request.payload_hex).map_err(bad_request)?;
    GsmtapEncodeInput::new(
        GsmtapHeader::new(
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
        ),
        &extension,
        &payload,
    )
    .map(|input| input.encode())
    .map_err(|error| bad_request(error.to_string()))
}

async fn send_record(
    state: &AppState,
    bytes: Vec<u8>,
    original: Option<String>,
    modified: bool,
) -> Result<Json<SendResponse>, (StatusCode, String)> {
    let destination = state
        .config
        .gsmtap_forward
        .ok_or_else(|| bad_request("a forward endpoint is required"))?;
    state
        .sender
        .send_to(&bytes, destination)
        .await
        .map_err(|error| (StatusCode::BAD_GATEWAY, format!("UDP send failed: {error}")))?;
    let mut record = match crate::gsmtap::parse(&bytes) {
        Ok(packet) => from_decoded("TX", "workbench".into(), &bytes, &packet),
        Err(error) => super::dto::from_error("TX", "workbench".into(), &bytes, error.to_string()),
    };
    record.mode = "modify".into();
    record.destination = Some(destination.to_string());
    record.modified = modified;
    record.original_raw_hex = original;
    record.final_raw_hex = Some(hex(&bytes));
    state.store.record(record).await;
    Ok(Json(SendResponse {
        timestamp_ms: std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis(),
        destination: destination.to_string(),
        encoded_hex: hex(&bytes),
    }))
}

fn bad_request(error: impl ToString) -> (StatusCode, String) {
    (StatusCode::BAD_REQUEST, error.to_string())
}

fn require_modify(state: &AppState) -> Result<(), (StatusCode, String)> {
    if state.config.mode == Mode::Modify {
        Ok(())
    } else {
        Err((
            StatusCode::FORBIDDEN,
            "packet replay and transmission are available only in modify mode".into(),
        ))
    }
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
