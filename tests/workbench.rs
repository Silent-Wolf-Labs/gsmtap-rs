use std::{sync::Arc, time::Duration};

use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use gsmtap_rs::workbench::{
    config::{Config, ConfigArgs, Mode},
    dto::from_decoded,
    history::PacketStore,
    network::{build_router, receive_loop},
};
use tokio::net::UdpSocket;
use tower::ServiceExt;

async fn wait_for_records(store: &PacketStore, count: usize) {
    tokio::time::timeout(Duration::from_secs(1), async {
        while store.list().await.len() < count {
            tokio::task::yield_now().await;
        }
    })
    .await
    .expect("packet history did not reach the expected size");
}

#[tokio::test]
async fn listen_mode_receives_without_allowing_transmission() {
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
        "02 04 01 00 00 01 00 00 00 00 00 01 00 00 00 00 CA FE"
    );

    let input = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":1,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA FE"});
    let response = router
        .clone()
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(input.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);
    let mut sent = [0u8; 64];
    assert!(
        tokio::time::timeout(Duration::from_millis(50), target.recv_from(&mut sent))
            .await
            .is_err()
    );
    let response = router
        .oneshot(
            Request::post("/api/packets/1/replay")
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);
    let malformed = [0xde, 0xad, 0xbe, 0xef];
    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&malformed, receiver_addr)
        .await
        .unwrap();
    wait_for_records(&store, 2).await;
    assert!(store.list().await[1].parse_error.is_some());
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
    wait_for_records(&store, 2).await;
    assert!(store
        .list()
        .await
        .iter()
        .all(|packet| packet.forward_status.as_deref() == Some("sent")));
    task.abort();
}

#[tokio::test]
async fn relay_mode_rejects_manual_packet_transmission() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let store = Arc::new(PacketStore::new(8));
    let sender = Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap());
    let config = Config::from_values(
        Mode::Relay,
        "127.0.0.1:0".parse().unwrap(),
        Some(target.local_addr().unwrap()),
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let router = build_router(config, store, sender);
    let input = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":1,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let response = router
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(input.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);
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
    wait_for_records(&store, 1).await;
    let id = store.list().await[0].id;
    let invalid = serde_json::json!({"version":2,"headerLengthWords":5,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let invalid_response = router
        .clone()
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-send"))
                .header("content-type", "application/json")
                .body(Body::from(invalid.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(invalid_response.status(), StatusCode::BAD_REQUEST);
    let mut received = [0u8; 64];
    assert!(
        tokio::time::timeout(Duration::from_millis(50), target.recv_from(&mut received))
            .await
            .is_err()
    );
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
    let (length, _) = target.recv_from(&mut received).await.unwrap();
    assert_eq!(&received[..length], original.as_slice());
    let edit = serde_json::json!({"version":2,"headerLengthWords":4,"messageType":1,"timeslot":0,"arfcn":2,"signalDbm":0,"snrDb":0,"frameNumber":1,"subtype":0,"antennaNumber":0,"subSlot":0,"reserved":0,"extensionHex":"","payloadHex":"CA"});
    let preview = router
        .clone()
        .oneshot(
            Request::post(format!("/api/packets/{id}/modify-preview"))
                .header("content-type", "application/json")
                .body(Body::from(edit.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(preview.status(), StatusCode::OK);
    let preview_body = hyper::body::to_bytes(preview.into_body()).await.unwrap();
    let preview_json: serde_json::Value = serde_json::from_slice(&preview_body).unwrap();
    assert_eq!(preview_json["fieldChanges"][0]["field"], "arfcn");
    assert!(
        tokio::time::timeout(Duration::from_millis(50), target.recv_from(&mut received))
            .await
            .is_err()
    );
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

#[test]
fn relay_and_modify_require_a_forward_endpoint() {
    for mode in [Mode::Relay, Mode::Modify] {
        let args = ConfigArgs {
            mode,
            gsmtap_listen: "127.0.0.1:4729".parse().unwrap(),
            gsmtap_forward: None,
            http_listen: "127.0.0.1:8080".parse().unwrap(),
            history_capacity: 8,
        };
        assert!(Config::try_from(args).is_err());
    }
}

#[tokio::test]
async fn relay_failure_is_recorded_and_does_not_stop_receiving() {
    let receiver = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let receiver_addr = receiver.local_addr().unwrap();
    let source = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let store = Arc::new(PacketStore::new(8));
    let sender = Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap());
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Relay,
        Some("255.255.255.255:4729".into()),
        sender,
    ));
    for _ in 0..2 {
        source
            .send_to(
                &[2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0],
                receiver_addr,
            )
            .await
            .unwrap();
    }
    wait_for_records(&store, 2).await;
    assert!(store.list().await.iter().all(|packet| packet
        .forward_status
        .as_deref()
        .is_some_and(|status| status.starts_with("error:"))));
    assert!(!task.is_finished());
    task.abort();
}

#[tokio::test]
async fn modify_send_failure_is_recorded() {
    let store = Arc::new(PacketStore::new(8));
    let raw = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0];
    let decoded = gsmtap_rs::gsmtap::parse(&raw).unwrap();
    store
        .record(from_decoded("RX", "127.0.0.1:1234".into(), &raw, &decoded))
        .await;
    let config = Config::from_values(
        Mode::Modify,
        "127.0.0.1:0".parse().unwrap(),
        Some("255.255.255.255:4729".parse().unwrap()),
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let router = build_router(
        config,
        store.clone(),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );
    let id = store.list().await[0].id;
    let response = router
        .oneshot(
            Request::post(format!("/api/packets/{id}/replay"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_GATEWAY);
    wait_for_records(&store, 2).await;
    let recorded = store.list().await;
    assert_eq!(recorded[1].direction, "TX");
    assert!(recorded[1]
        .forward_status
        .as_deref()
        .is_some_and(|status| status.starts_with("error:")));
}

#[tokio::test]
async fn status_reports_the_active_mode_and_endpoints() {
    let forward: std::net::SocketAddr = "127.0.0.1:14729".parse().unwrap();
    let config = Config::from_values(
        Mode::Modify,
        "127.0.0.1:14728".parse().unwrap(),
        Some(forward),
        "127.0.0.1:18080".parse().unwrap(),
        8,
    );
    let router = build_router(
        config,
        Arc::new(PacketStore::new(8)),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );
    let response = router
        .oneshot(Request::get("/api/status").body(Body::empty()).unwrap())
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
    let status: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(status["mode"], "modify");
    assert_eq!(status["gsmtapForward"], forward.to_string());
}

#[tokio::test]
async fn history_evicts_oldest_packets_and_counts_evictions() {
    let receiver = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let receiver_addr = receiver.local_addr().unwrap();
    let source = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let store = Arc::new(PacketStore::new(1));
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    ));
    for payload in [0xca, 0xcb] {
        source
            .send_to(
                &[2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, payload],
                receiver_addr,
            )
            .await
            .unwrap();
    }
    tokio::time::timeout(Duration::from_secs(1), async {
        while {
            let stats = store.counters().snapshot();
            stats.received < 2 || stats.history_dropped < 1
        } {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    let history = store.list().await;
    assert_eq!(history.len(), 1);
    assert!(history[0].raw_hex.ends_with("CB"));
    assert_eq!(store.counters().snapshot().history_dropped, 1);
    task.abort();
}

#[tokio::test]
async fn packet_api_returns_bounded_recent_pages_in_chronological_order() {
    let store = Arc::new(PacketStore::new(1_200));
    for byte in 0..1_105u16 {
        store
            .record(gsmtap_rs::workbench::dto::from_error(
                "RX",
                "127.0.0.1:4729".into(),
                &[byte as u8],
                "test packet".into(),
            ))
            .await;
    }
    let router = build_router(
        Config::from_values(
            Mode::Listen,
            "127.0.0.1:4729".parse().unwrap(),
            None,
            "127.0.0.1:8080".parse().unwrap(),
            1_200,
        ),
        store,
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );

    async fn ids(router: &axum::Router, path: &str) -> Vec<u64> {
        let response = router
            .clone()
            .oneshot(Request::get(path).body(Body::empty()).unwrap())
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
        serde_json::from_slice::<Vec<serde_json::Value>>(&body)
            .unwrap()
            .into_iter()
            .map(|packet| packet["id"].as_u64().unwrap())
            .collect()
    }

    let default_ids = ids(&router, "/api/packets").await;
    assert_eq!(default_ids.len(), 500);
    assert_eq!(default_ids.first(), Some(&606));
    assert_eq!(default_ids.last(), Some(&1_105));

    let short_ids = ids(&router, "/api/packets?limit=3").await;
    assert_eq!(short_ids, vec![1_103, 1_104, 1_105]);

    let capped_ids = ids(&router, "/api/packets?limit=5000").await;
    assert_eq!(capped_ids.len(), 1_000);
    assert_eq!(capped_ids.first(), Some(&106));
    assert_eq!(capped_ids.last(), Some(&1_105));

    let invalid = router
        .oneshot(
            Request::get("/api/packets?limit=not-a-number")
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(invalid.status(), StatusCode::BAD_REQUEST);
}
