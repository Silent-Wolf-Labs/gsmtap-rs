use std::sync::Arc;

use axum::{
    extract::{Path, Query, State},
    http::StatusCode,
    response::sse::{Event, KeepAlive, Sse},
    response::Html,
    Json,
};
use tokio_stream::{
    wrappers::{errors::BroadcastStreamRecvError, BroadcastStream},
    StreamExt,
};

use crate::gsmtap::{GsmtapEncodeInput, GsmtapHeader};

use super::{
    capture::CaptureControl,
    config::{Config, Mode},
    dto::{
        decoded_from_packet, from_decoded, from_error, hex, parse_hex, DecodedPacket,
        EncodeSendRequest, PacketRecord, SendResponse,
    },
    history::{PacketStore, RuntimeStats},
};

#[derive(Clone)]
pub struct AppState {
    pub config: Config,
    pub store: Arc<PacketStore>,
    pub sender: Arc<tokio::net::UdpSocket>,
    pub capture: CaptureControl,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct StatusResponse {
    pub gsmtap_listen: String,
    pub mode: Mode,
    pub gsmtap_forward: Option<String>,
    pub http_listen: String,
    pub receive_state: &'static str,
    pub capture_paused: bool,
    pub stats: RuntimeStats,
}

#[derive(serde::Deserialize)]
pub struct PacketQuery {
    pub limit: Option<usize>,
}

const DEFAULT_PACKET_LIMIT: usize = 500;
const MAX_PACKET_LIMIT: usize = 1_000;

pub async fn status(State(state): State<AppState>) -> Json<StatusResponse> {
    Json(StatusResponse {
        gsmtap_listen: state.config.gsmtap_listen.to_string(),
        mode: state.config.mode,
        gsmtap_forward: state.config.gsmtap_forward.clone(),
        http_listen: state.config.http_listen.to_string(),
        receive_state: "listening",
        capture_paused: state.capture.is_paused(),
        stats: state.store.counters().snapshot(),
    })
}

#[derive(serde::Deserialize)]
pub struct CaptureRequest {
    pub paused: bool,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CaptureResponse {
    pub capture_paused: bool,
}

pub async fn set_capture(
    State(state): State<AppState>,
    Json(request): Json<CaptureRequest>,
) -> Json<CaptureResponse> {
    state.capture.set_paused(request.paused);
    Json(CaptureResponse {
        capture_paused: request.paused,
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
        include_str!("../../static/styles/style.css"),
    )
}

pub async fn packet_table_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/packet/packet-table.js"),
    )
}

pub async fn filters_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/filters.js"),
    )
}

pub async fn select_control_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/select-control.js"),
    )
}

pub async fn api_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/services/api.js"),
    )
}

pub async fn events_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/services/events.js"),
    )
}

pub async fn app_controller_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/controllers/app-controller.js"),
    )
}

pub async fn mode_panel_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/panels/mode-panel.js"),
    )
}

pub async fn listen_panel_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/panels/listen-panel.js"),
    )
}

pub async fn relay_panel_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/panels/relay-panel.js"),
    )
}

pub async fn packet_schema_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/packet/packet-schema.js"),
    )
}

pub async fn packet_model_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/models/packet-model.js"),
    )
}

pub async fn tooltip_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/tooltip.js"),
    )
}

pub async fn card_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/cards/card.js"),
    )
}

pub async fn status_card_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/cards/status-card.js"),
    )
}

pub async fn modify_panel_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/panels/modify-panel.js"),
    )
}

pub async fn modify_model_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/models/modify-model.js"),
    )
}

pub async fn modify_fields_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/modify/modify-fields.js"),
    )
}

pub async fn modify_form_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/modify/modify-form.js"),
    )
}

pub async fn modify_preview_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/modify/modify-preview.js"),
    )
}

pub async fn modify_state_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/modify/modify-state.js"),
    )
}

pub async fn modify_validation_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/components/modify/modify-validation.js"),
    )
}

pub async fn modification_service_javascript() -> (
    [(axum::http::header::HeaderName, &'static str); 1],
    &'static str,
) {
    (
        [(axum::http::header::CONTENT_TYPE, "application/javascript")],
        include_str!("../../static/services/modification-service.js"),
    )
}

pub async fn packets(
    State(state): State<AppState>,
    Query(query): Query<PacketQuery>,
) -> Json<Vec<PacketRecord>> {
    let limit = query
        .limit
        .unwrap_or(DEFAULT_PACKET_LIMIT)
        .min(MAX_PACKET_LIMIT);
    Json(state.store.list_recent(limit).await)
}

pub async fn encode_send(
    State(state): State<AppState>,
    Json(request): Json<EncodeSendRequest>,
) -> Result<Json<SendResponse>, (StatusCode, String)> {
    require_modify(&state)?;
    send_record(&state, encode_request(&request)?, None, true).await
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
    if packet.decoded.is_none() {
        return Err(bad_request(
            "only successfully decoded packets can be modified",
        ));
    }
    let original = packet.raw_hex.clone();
    let bytes = encode_request(&request)?;
    send_record(&state, bytes, Some(original), true).await
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ModifyPreview {
    pub packet_id: u64,
    pub original_hex: String,
    pub modified_hex: String,
    pub original_decoded: DecodedPacket,
    pub modified_decoded: DecodedPacket,
    pub field_changes: Vec<FieldChange>,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FieldChange {
    pub field: &'static str,
    pub original: String,
    pub modified: String,
}

pub async fn modify_preview(
    State(state): State<AppState>,
    Path(id): Path<u64>,
    Json(request): Json<EncodeSendRequest>,
) -> Result<Json<ModifyPreview>, (StatusCode, String)> {
    require_modify(&state)?;
    let packet = state
        .store
        .get(id)
        .await
        .ok_or((StatusCode::NOT_FOUND, "packet not found".into()))?;
    let original = packet
        .decoded
        .as_ref()
        .ok_or_else(|| bad_request("only successfully decoded packets can be modified"))?;
    let encoded = encode_request(&request)?;
    let modified = crate::gsmtap::parse(&encoded)
        .map_err(|error| (StatusCode::INTERNAL_SERVER_ERROR, error.to_string()))?;
    let modified_decoded = decoded_from_packet(&modified);
    let mut changes = Vec::new();
    let header = modified.header();
    push_change(&mut changes, "version", original.version, header.version());
    push_change(
        &mut changes,
        "headerLengthWords",
        original.header_length_words,
        header.header_length_words(),
    );
    push_change(
        &mut changes,
        "messageType",
        original.message_type,
        header.message_type(),
    );
    push_change(
        &mut changes,
        "timeslot",
        original.timeslot,
        header.timeslot(),
    );
    push_change(&mut changes, "arfcn", original.arfcn, header.arfcn());
    push_change(
        &mut changes,
        "signalDbm",
        original.signal_dbm,
        header.signal_dbm(),
    );
    push_change(&mut changes, "snrDb", original.snr_db, header.snr_db());
    push_change(
        &mut changes,
        "frameNumber",
        original.frame_number,
        header.frame_number(),
    );
    push_change(&mut changes, "subtype", original.subtype, header.subtype());
    push_change(
        &mut changes,
        "antennaNumber",
        original.antenna_number,
        header.antenna_number(),
    );
    push_change(
        &mut changes,
        "subSlot",
        original.sub_slot,
        header.sub_slot(),
    );
    push_change(
        &mut changes,
        "reserved",
        original.reserved,
        header.reserved(),
    );
    push_change(
        &mut changes,
        "extensionHex",
        &original.extension_hex,
        &hex(modified.extension()),
    );
    push_change(
        &mut changes,
        "payloadHex",
        &original.payload_hex,
        &hex(modified.payload()),
    );
    Ok(Json(ModifyPreview {
        packet_id: id,
        original_hex: packet.raw_hex,
        modified_hex: hex(&encoded),
        original_decoded: original.clone(),
        modified_decoded,
        field_changes: changes,
    }))
}

fn push_change<T: ToString + PartialEq>(
    changes: &mut Vec<FieldChange>,
    field: &'static str,
    original: T,
    modified: T,
) {
    if original != modified {
        changes.push(FieldChange {
            field,
            original: original.to_string(),
            modified: modified.to_string(),
        });
    }
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
        .clone()
        .ok_or_else(|| bad_request("a forward endpoint is required"))?;
    let mut record = match crate::gsmtap::parse(&bytes) {
        Ok(packet) => from_decoded("TX", Mode::Modify, "workbench".into(), &bytes, &packet),
        Err(error) => from_error(
            "TX",
            Mode::Modify,
            "workbench".into(),
            &bytes,
            error.to_string(),
        ),
    };
    record.destination = Some(destination.clone());
    record.source_address = state
        .sender
        .local_addr()
        .ok()
        .map(|address| address.to_string());
    record.modified = modified;
    record.original_raw_hex = original;
    record.final_raw_hex = Some(hex(&bytes));
    let destination = match tokio::net::lookup_host(&destination).await {
        Ok(mut addresses) => match addresses.find(std::net::SocketAddr::is_ipv4) {
            Some(address) => address,
            None => {
                let message = "UDP destination resolved to no IPv4 addresses".to_string();
                record.forward_status = Some(format!("error: {message}"));
                state.store.record(record).await;
                return Err((StatusCode::BAD_GATEWAY, message));
            }
        },
        Err(error) => {
            let message = format!("UDP destination lookup failed: {error}");
            record.forward_status = Some(format!("error: {message}"));
            state.store.record(record).await;
            return Err((StatusCode::BAD_GATEWAY, message));
        }
    };
    if let Err(error) = state.sender.send_to(&bytes, destination).await {
        record.forward_status = Some(format!("error: {error}"));
        state.store.record(record).await;
        return Err((StatusCode::BAD_GATEWAY, format!("UDP send failed: {error}")));
    }
    record.forward_status = Some("sent".into());
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
    let store = state.store.clone();
    let stream = BroadcastStream::new(state.store.subscribe())
        .filter_map(move |packet| match packet {
            Ok(packet) => Some(packet),
            Err(BroadcastStreamRecvError::Lagged(count)) => {
                store.counters().ui_events_dropped(count);
                None
            }
        })
        .map(|packet| {
            Ok(Event::default()
                .json_data(packet)
                .expect("packet DTO is serializable"))
        });
    Sse::new(stream).keep_alive(KeepAlive::default())
}
