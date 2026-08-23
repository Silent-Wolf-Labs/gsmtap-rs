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
