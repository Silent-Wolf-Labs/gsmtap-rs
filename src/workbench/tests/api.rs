use super::super::{
    config::{Config, Mode},
    history::PacketStore,
    network::build_router,
};
use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use std::sync::Arc;
use tokio::net::UdpSocket;
use tower::ServiceExt;

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
    ] {
        let response = router
            .clone()
            .oneshot(Request::get(path).body(Body::empty()).unwrap())
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK, "{path}");
        assert_eq!(response.headers()["content-type"], "application/javascript", "{path}");
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
                .body(Body::from(serde_json::json!({ "packetIds": [] }).to_string()))
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
                .body(Body::from(serde_json::json!({ "packetIds": [1, 2, 1] }).to_string()))
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
                .body(Body::from(serde_json::json!({ "packetIds": large_ids }).to_string()))
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
