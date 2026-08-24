use super::super::{
    config::Mode,
    network::{build_router, receive_loop},
};
use super::{config, socket, store, wait_for_records};
use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use std::sync::Arc;
use tokio::net::UdpSocket;
use tower::ServiceExt;

const PACKET: [u8; 17] = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca];

fn valid_encode_request() -> serde_json::Value {
    serde_json::json!({
        "version": 2, "headerLengthWords": 4, "messageType": 1,
        "timeslot": 0, "arfcn": 1, "signalDbm": 0, "snrDb": 0,
        "frameNumber": 1, "subtype": 0, "antennaNumber": 0,
        "subSlot": 0, "reserved": 0, "extensionHex": "", "payloadHex": "CA FE"
    })
}

#[tokio::test]
async fn listen_mode_receives_without_allowing_transmission() {
    let target = socket().await;
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let store = store();
    let router = build_router(
        config(
            Mode::Listen,
            "127.0.0.1:0".parse().unwrap(),
            Some(target.local_addr().unwrap().to_string()),
        ),
        store.clone(),
        Arc::new(socket().await),
    );
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        Arc::new(socket().await),
    ));
    socket()
        .await
        .send_to(&PACKET, receiver_addr)
        .await
        .unwrap();
    wait_for_records(&store, 1).await;
    let response = router
        .clone()
        .oneshot(Request::get("/api/packets").body(Body::empty()).unwrap())
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(json[0]["direction"], "RX");
    assert_eq!(
        json[0]["rawHex"],
        "02 04 01 00 00 01 00 00 00 00 00 01 00 00 00 00 CA"
    );
    let response = router
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(valid_encode_request().to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);
    task.abort();
}

#[tokio::test]
async fn relay_forwards_valid_and_malformed_datagrams_unchanged() {
    let target = socket().await;
    let receiver = socket().await;
    let source = socket().await;
    let store = store();
    let forward = target.local_addr().unwrap().to_string();
    let listen = receiver.local_addr().unwrap();
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Relay,
        Some(forward),
        Arc::new(socket().await),
    ));
    for expected in [PACKET.to_vec(), vec![0xde, 0xad, 0xbe, 0xef]] {
        source.send_to(&expected, listen).await.unwrap();
        let mut received = [0u8; 64];
        let (length, _) = target.recv_from(&mut received).await.unwrap();
        assert_eq!(&received[..length], expected.as_slice());
    }
    wait_for_records(&store, 2).await;
    assert!(store
        .list()
        .await
        .iter()
        .all(|packet| packet.forward_status.as_deref() == Some("sent")));
    let stats = store.counters().snapshot();
    assert_eq!(stats.forward_sent, 2);
    assert_eq!(stats.forward_failed, 0);
    task.abort();
}

#[tokio::test]
async fn relay_mode_rejects_manual_packet_transmission() {
    let target = socket().await;
    let store = store();
    let router = build_router(
        config(
            Mode::Relay,
            "127.0.0.1:0".parse().unwrap(),
            Some(target.local_addr().unwrap().to_string()),
        ),
        store,
        Arc::new(socket().await),
    );
    let response = router
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(valid_encode_request().to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);
}

async fn modify_fixture() -> (
    axum::Router,
    Arc<super::super::history::PacketStore>,
    UdpSocket,
    u64,
) {
    let target = socket().await;
    let receiver = socket().await;
    let source = socket().await;
    let store = store();
    let config = config(
        Mode::Modify,
        receiver.local_addr().unwrap(),
        Some(target.local_addr().unwrap().to_string()),
    );
    let router = build_router(config.clone(), store.clone(), Arc::new(socket().await));
    tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Modify,
        config.gsmtap_forward.clone(),
        Arc::new(socket().await),
    ));
    source.send_to(&PACKET, config.gsmtap_listen).await.unwrap();
    wait_for_records(&store, 1).await;
    let id = store.list().await[0].id;
    (router, store, target, id)
}

#[tokio::test]
async fn modify_mode_rejects_invalid_modification() {
    let (router, _, target, id) = modify_fixture().await;
    let invalid = serde_json::json!({"version":2,"headerLengthWords":5,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let response = router
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-send"))
                .header("content-type", "application/json")
                .body(Body::from(invalid.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_REQUEST);
    let mut received = [0u8; 64];
    assert!(tokio::time::timeout(
        std::time::Duration::from_millis(50),
        target.recv_from(&mut received)
    )
    .await
    .is_err());
}

#[tokio::test]
async fn modify_preview_does_not_transmit() {
    let (router, _, target, id) = modify_fixture().await;
    let edit = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let response = router
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-preview"))
                .header("content-type", "application/json")
                .body(Body::from(edit.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let mut received = [0u8; 64];
    assert!(tokio::time::timeout(
        std::time::Duration::from_millis(50),
        target.recv_from(&mut received)
    )
    .await
    .is_err());
}

#[tokio::test]
async fn modify_send_and_replay_transmit_expected_packets() {
    let (router, store, target, id) = modify_fixture().await;
    let replay = router
        .clone()
        .oneshot(
            Request::post(format!("/api/packets/{id}/replay"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(replay.status(), StatusCode::OK);
    let mut received = [0u8; 64];
    let (length, _) = target.recv_from(&mut received).await.unwrap();
    assert_eq!(&received[..length], &PACKET);
    let edit = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let response = router
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-send"))
                .header("content-type", "application/json")
                .body(Body::from(edit.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let (length, _) = target.recv_from(&mut received).await.unwrap();
    assert_eq!(
        &received[..length],
        &[2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca]
    );
    assert!(store.list().await.iter().any(|packet| packet.modified
        && packet.original_raw_hex.as_deref()
            == Some("02 04 01 00 00 01 00 00 00 00 00 01 00 00 00 00 CA")));
}
