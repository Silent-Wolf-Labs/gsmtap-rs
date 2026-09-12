use super::super::{
    capture::CaptureControl,
    config::{Config, Mode},
    history::PacketStore,
    network::{build_router, build_router_with_runtime},
    runtime::{PipelineFactory, RuntimeHandle},
};
use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use std::sync::Arc;
use tokio::net::UdpSocket;
use tower::ServiceExt;

const PACKET: [u8; 17] = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca];

struct RejectRelayPipeline;

impl PipelineFactory for RejectRelayPipeline {
    fn preflight(&self, mode: Mode, _forward: Option<&str>) -> Result<(), std::io::Error> {
        if mode == Mode::Relay {
            Err(std::io::Error::other("replacement startup failed"))
        } else {
            Ok(())
        }
    }
}

async fn put_mode(router: axum::Router, mode: &str) -> axum::response::Response {
    put_mode_with_forward(router, mode, None).await
}

async fn put_mode_with_forward(
    router: axum::Router,
    mode: &str,
    forward_address: Option<&str>,
) -> axum::response::Response {
    router
        .oneshot(
            Request::put("/api/mode")
                .header("content-type", "application/json")
                .body(Body::from(
                    serde_json::json!({ "mode": mode, "forwardAddress": forward_address })
                        .to_string(),
                ))
                .unwrap(),
        )
        .await
        .unwrap()
}

#[tokio::test]
async fn mode_transition_sets_retains_and_clears_the_runtime_forward_address() {
    let config = Config::from_values(
        Mode::Listen,
        "127.0.0.1:0".parse().unwrap(),
        None,
        "127.0.0.1:0".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let store = Arc::new(PacketStore::new(8));
    let runtime = RuntimeHandle::start(config.clone(), store.clone(), CaptureControl::default())
        .await
        .unwrap();
    let router = build_router_with_runtime(
        config,
        store,
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
        CaptureControl::default(),
        runtime.clone(),
    );

    let response = put_mode_with_forward(router.clone(), "relay", Some("127.0.0.1:14729")).await;
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(runtime.mode().await, Mode::Relay);
    assert_eq!(runtime.forward().await.as_deref(), Some("127.0.0.1:14729"));

    let response = put_mode(router.clone(), "modify").await;
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(runtime.mode().await, Mode::Modify);
    assert_eq!(runtime.forward().await.as_deref(), Some("127.0.0.1:14729"));

    let response = put_mode(router, "listen").await;
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(runtime.mode().await, Mode::Listen);
    assert_eq!(runtime.forward().await, None);
}

async fn wait_for_records(store: &PacketStore, count: usize) {
    tokio::time::timeout(std::time::Duration::from_secs(1), async {
        while store.list().await.len() < count {
            tokio::task::yield_now().await;
        }
    })
    .await
    .expect("packet history did not reach the expected size");
}

#[tokio::test]
async fn mode_api_switches_live_pipeline_clears_history_and_updates_authorization() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let config = Config::from_values(
        Mode::Listen,
        "127.0.0.1:0".parse().unwrap(),
        Some(target.local_addr().unwrap().to_string()),
        "127.0.0.1:0".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let store = Arc::new(PacketStore::new(8));
    let runtime = RuntimeHandle::start(config.clone(), store.clone(), CaptureControl::default())
        .await
        .unwrap();
    let listener = runtime.listen_addr().unwrap();
    let router = build_router_with_runtime(
        config,
        store.clone(),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
        CaptureControl::default(),
        runtime.clone(),
    );

    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&PACKET, listener)
        .await
        .unwrap();
    wait_for_records(&store, 1).await;
    let response = put_mode(router.clone(), "relay").await;
    assert_eq!(response.status(), StatusCode::OK);
    assert!(store.list().await.is_empty());
    assert_eq!(runtime.mode().await, Mode::Relay);
    assert_eq!(runtime.listen_addr(), Some(listener));

    let response = router
        .clone()
        .oneshot(
            Request::post("/api/encode-send")
                .header("content-type", "application/json")
                .body(Body::from(
                    serde_json::json!({
                        "version": 2, "headerLengthWords": 4, "messageType": 1,
                        "timeslot": 0, "arfcn": 1, "signalDbm": 0, "snrDb": 0,
                        "frameNumber": 1, "subtype": 0, "antennaNumber": 0,
                        "subSlot": 0, "reserved": 0,
                    })
                    .to_string(),
                ))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::FORBIDDEN);

    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&PACKET, listener)
        .await
        .unwrap();
    wait_for_records(&store, 1).await;
    assert_eq!(store.list().await[0].mode, Mode::Relay);
}

#[tokio::test]
async fn failed_mode_preflight_retains_listener_mode_and_packet_history() {
    let config = Config::from_values(
        Mode::Listen,
        "127.0.0.1:0".parse().unwrap(),
        Some("127.0.0.1:4729".into()),
        "127.0.0.1:0".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let store = Arc::new(PacketStore::new(8));
    let runtime = RuntimeHandle::start_with_test_factory(
        config.clone(),
        store.clone(),
        CaptureControl::default(),
        Arc::new(RejectRelayPipeline),
    )
    .await
    .unwrap();
    let listener = runtime.listen_addr().unwrap();
    let router = build_router_with_runtime(
        config,
        store.clone(),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
        CaptureControl::default(),
        runtime.clone(),
    );

    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&PACKET, listener)
        .await
        .unwrap();
    wait_for_records(&store, 1).await;
    let response = put_mode(router, "relay").await;
    assert_eq!(response.status(), StatusCode::INTERNAL_SERVER_ERROR);
    assert_eq!(runtime.mode().await, Mode::Listen);
    assert_eq!(runtime.listen_addr(), Some(listener));
    assert_eq!(store.list().await.len(), 1);

    UdpSocket::bind("127.0.0.1:0")
        .await
        .unwrap()
        .send_to(&PACKET, listener)
        .await
        .unwrap();
    wait_for_records(&store, 2).await;
    assert!(store
        .list()
        .await
        .iter()
        .all(|record| record.mode == Mode::Listen));
}

#[tokio::test]
async fn concurrent_mode_changes_are_serialized_in_request_order() {
    let config = Config::from_values(
        Mode::Listen,
        "127.0.0.1:0".parse().unwrap(),
        Some("127.0.0.1:4729".into()),
        "127.0.0.1:0".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let runtime = RuntimeHandle::start(
        config,
        Arc::new(PacketStore::new(8)),
        CaptureControl::default(),
    )
    .await
    .unwrap();

    let (relay, modify) = tokio::join!(
        runtime.change_mode(Mode::Relay, None),
        runtime.change_mode(Mode::Modify, None),
    );

    assert_eq!(relay.unwrap().0, Mode::Relay);
    assert_eq!(modify.unwrap().0, Mode::Modify);
    assert_eq!(runtime.mode().await, Mode::Modify);
}

#[tokio::test]
async fn status_reports_the_active_mode_and_endpoints() {
    let forward = "127.0.0.1:14729";
    let config = Config::from_values(
        Mode::Modify,
        "127.0.0.1:14728".parse().unwrap(),
        Some(forward.into()),
        "127.0.0.1:18080".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
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
    assert_eq!(status["gsmtapForward"], forward);
}

#[tokio::test]
async fn stylesheet_entry_point_and_imported_assets_are_served() {
    let router = build_router(
        Config::from_values(
            Mode::Listen,
            "127.0.0.1:4729".parse().unwrap(),
            None,
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .unwrap(),
        Arc::new(PacketStore::new(8)),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );
    let assets = [
        ("/styles/style.css", "@import \"./base.css\";"),
        ("/styles/base.css", ":root"),
        ("/styles/forms.css", "button"),
        ("/styles/status-card.css", ".status-card"),
        ("/styles/modify.css", ".modify-fields"),
        ("/styles/packet-table.css", "table"),
    ];

    for (path, expected_content) in assets {
        let response = router
            .clone()
            .oneshot(Request::get(path).body(Body::empty()).unwrap())
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK, "{path}");
        assert_eq!(response.headers()["content-type"], "text/css", "{path}");
        let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
        let body = String::from_utf8(body.to_vec()).unwrap();
        assert!(body.contains(expected_content), "{path}");
    }

    let response = router
        .clone()
        .oneshot(
            Request::get("/components/packet/column-filter.js")
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(response.headers()["content-type"], "application/javascript");

    for path in [
        "/components/packet/select-filter.js",
        "/components/packet/text-filter.js",
        "/controllers/packet-history-controller.js",
        "/components/packet/packet-history-view.js",
        "/components/dialogs/clear-history-dialog.js",
    ] {
        let response = router
            .clone()
            .oneshot(Request::get(path).body(Body::empty()).unwrap())
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK, "{path}");
        assert_eq!(
            response.headers()["content-type"],
            "application/javascript",
            "{path}"
        );
    }
}

#[tokio::test]
async fn packet_api_returns_bounded_recent_pages_in_chronological_order() {
    let store = Arc::new(PacketStore::new(1_200));
    for byte in 0..1_105u16 {
        store
            .record(super::super::dto::from_error(
                "RX",
                Mode::Listen,
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
            1_024,
        )
        .unwrap(),
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
        serde_json::from_slice::<Vec<serde_json::Value>>(
            &hyper::body::to_bytes(response.into_body()).await.unwrap(),
        )
        .unwrap()
        .into_iter()
        .map(|packet| packet["id"].as_u64().unwrap())
        .collect()
    }
    let default_ids = ids(&router, "/api/packets").await;
    assert_eq!(default_ids.len(), 500);
    assert_eq!(default_ids.first(), Some(&606));
    assert_eq!(default_ids.last(), Some(&1_105));
    assert_eq!(
        ids(&router, "/api/packets?limit=3").await,
        vec![1_103, 1_104, 1_105]
    );
    assert_eq!(ids(&router, "/api/packets?limit=5000").await.len(), 1_000);
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

#[tokio::test]
async fn forward_packets_rejects_non_relay_mode() {
    for mode in [Mode::Listen, Mode::Modify] {
        let forward = "127.0.0.1:14729";
        let config = Config::from_values(
            mode,
            "127.0.0.1:14728".parse().unwrap(),
            Some(forward.into()),
            "127.0.0.1:18080".parse().unwrap(),
            8,
            16,
        )
        .unwrap();
        let router = build_router(
            config,
            Arc::new(PacketStore::new(8)),
            Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
        );
        let request_body = serde_json::json!({ "packetIds": [1] });
        let response = router
            .oneshot(
                Request::post("/api/packets/forward")
                    .header("content-type", "application/json")
                    .body(Body::from(request_body.to_string()))
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::FORBIDDEN);
    }
}

#[tokio::test]
async fn forward_packets_validates_request_payload() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let config = Config::from_values(
        Mode::Relay,
        "127.0.0.1:14728".parse().unwrap(),
        Some(target.local_addr().unwrap().to_string()),
        "127.0.0.1:18080".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let router = build_router(
        config,
        Arc::new(PacketStore::new(8)),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );

    // Empty packetIds
    let response = router
        .clone()
        .oneshot(
            Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(Body::from(
                    serde_json::json!({ "packetIds": [] }).to_string(),
                ))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_REQUEST);

    // Duplicate packetIds
    let response = router
        .clone()
        .oneshot(
            Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(Body::from(
                    serde_json::json!({ "packetIds": [1, 2, 1] }).to_string(),
                ))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_REQUEST);

    // Oversized batch (>1000 items)
    let large_ids: Vec<u64> = (1..=1001).collect();
    let response = router
        .clone()
        .oneshot(
            Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(Body::from(
                    serde_json::json!({ "packetIds": large_ids }).to_string(),
                ))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_REQUEST);

    // Malformed JSON
    let response = router
        .oneshot(
            Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(Body::from("invalid json"))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::BAD_REQUEST);
}

#[tokio::test]
async fn forward_packets_handles_unknown_ids_and_transmits_valid_packets() {
    let target = UdpSocket::bind("127.0.0.1:0").await.unwrap();
    let target_addr = target.local_addr().unwrap();
    let store = Arc::new(PacketStore::new(8));
    let raw = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca];
    let decoded = crate::gsmtap::parse(&raw).unwrap();
    store
        .record(super::super::dto::from_decoded(
            "RX",
            Mode::Relay,
            "127.0.0.1:1234".into(),
            &raw,
            &decoded,
        ))
        .await;
    let packet_id = store.list().await[0].id;

    let config = Config::from_values(
        Mode::Relay,
        "127.0.0.1:14728".parse().unwrap(),
        Some(target_addr.to_string()),
        "127.0.0.1:18080".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let router = build_router(
        config,
        store.clone(),
        Arc::new(UdpSocket::bind("127.0.0.1:0").await.unwrap()),
    );

    let request_body = serde_json::json!({ "packetIds": [999, packet_id, 888] });
    let response = router
        .oneshot(
            Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(Body::from(request_body.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(
        json["results"],
        serde_json::json!([
            { "packetId": 999, "status": "not found" },
            { "packetId": packet_id, "status": "sent" },
            { "packetId": 888, "status": "not found" }
        ])
    );

    let mut buf = [0u8; 64];
    let (length, _) = target.recv_from(&mut buf).await.unwrap();
    assert_eq!(&buf[..length], &raw);

    let record = store.get(packet_id).await.unwrap();
    assert_eq!(record.forward_status.as_deref(), Some("sent"));
    let stats = store.counters().snapshot();
    assert_eq!(stats.forward_sent, 1);
    assert_eq!(stats.forward_failed, 0);
}
