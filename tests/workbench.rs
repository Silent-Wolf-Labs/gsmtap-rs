use std::sync::Arc;

use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use gsmtap_rs::workbench::{
    config::{Config, Mode},
    history::PacketStore,
    network::{build_router, receive_loop},
};
use tokio::net::UdpSocket;
use tower::ServiceExt;

#[tokio::test]
async fn rx_and_tx_paths_use_the_gsmtap_library() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let target_addr = target.local_addr().unwrap();
    let receiver = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let receiver_addr = receiver.local_addr().unwrap();
    let store = Arc::new(PacketStore::new(8));
    let sender = Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap());
    let config = Config::from_values(
        Mode::Listen,
        "127.0.0.1:0".parse().unwrap(),
        target_addr.into(),
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let router = build_router(config, store.clone(), sender);
    let receive_task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    ));

    let packet = vec![2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca, 0xfe];
    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&packet, receiver_addr)
        .await
        .unwrap();
    for _ in 0..20 {
        if store.list().await.len() == 1 {
            break;
        }
        tokio::task::yield_now().await;
    }
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
        "02 04 01 00 00 01 00 00 00 00 00 01 00 00 00 00 CA FE"
    );

    let input = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":1,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA FE"});
    let response = router
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(input.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let mut sent = [0u8; 64];
    let (length, _) = target.recv_from(&mut sent).await.unwrap();
    assert_eq!(&sent[..length], packet.as_slice());
    assert_eq!(
        store
            .list()
            .await
            .iter()
            .filter(|item| item.direction == "TX")
            .count(),
        1
    );
    receive_task.abort();
}

#[tokio::test]
async fn relay_forwards_valid_and_malformed_datagrams_unchanged() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let receiver = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let source = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let store = Arc::new(PacketStore::new(8));
    let sender = Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap());
    let config = Config::from_values(
        Mode::Relay,
        receiver.local_addr().unwrap(),
        Some(target.local_addr().unwrap()),
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Relay,
        config.gsmtap_forward,
        sender,
    ));
    let packets = [
        vec![2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca],
        vec![0xde, 0xad, 0xbe, 0xef],
    ];
    for expected in packets {
        source
            .send_to(&expected, config.gsmtap_listen)
            .await
            .unwrap();
        let mut received = [0u8; 64];
        let (length, _) = target.recv_from(&mut received).await.unwrap();
        assert_eq!(&received[..length], expected.as_slice());
    }
    assert_eq!(store.list().await.len(), 2);
    assert!(store
        .list()
        .await
        .iter()
        .all(|packet| packet.forward_status.as_deref() == Some("sent")));
    task.abort();
}

#[tokio::test]
async fn modify_mode_replays_and_reencodes_only_on_explicit_request() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let receiver = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let source = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let store = Arc::new(PacketStore::new(8));
    let sender = Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap());
    let config = Config::from_values(
        Mode::Modify,
        receiver.local_addr().unwrap(),
        Some(target.local_addr().unwrap()),
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let router = build_router(config.clone(), store.clone(), sender);
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Modify,
        config.gsmtap_forward,
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    ));
    let original = vec![2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca];
    source
        .send_to(&original, config.gsmtap_listen)
        .await
        .unwrap();
    for _ in 0..20 {
        if store.list().await.len() == 1 {
            break;
        }
        tokio::task::yield_now().await;
    }
    let id = store.list().await[0].id;
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
    assert_eq!(&received[..length], original.as_slice());
    let edit = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let modified = router
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-send"))
                .header("content-type", "application/json")
                .body(Body::from(edit.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(modified.status(), StatusCode::OK);
    let (length, _) = target.recv_from(&mut received).await.unwrap();
    assert_eq!(&received[4..6], &[0, 2]);
    assert_eq!(
        &received[..length],
        &[2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca]
    );
    assert!(store.list().await.iter().any(|packet| packet.modified
        && packet.original_raw_hex.as_deref()
            == Some("02 04 01 00 00 01 00 00 00 00 00 01 00 00 00 00 CA")));
    task.abort();
}
