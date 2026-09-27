use super::super::{config::Mode, dto::from_decoded, network::receive_loop};
use super::{config, socket, store, wait_for_records};
use std::sync::Arc;
use tower::ServiceExt;

#[tokio::test]
async fn relay_ingress_records_without_forwarding_and_does_not_stop_receiving() {
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = store();
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Relay,
        Some("255.255.255.255:4729".into()),
        Arc::new(socket().await),
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
    assert!(store
        .list()
        .await
        .iter()
        .all(|packet| packet.forward_status.is_none()));
    assert_eq!(store.counters().snapshot().forward_failed, 0);
    assert_eq!(store.counters().snapshot().forward_sent, 0);
    assert!(!task.is_finished());
    task.abort();
}

#[tokio::test]
async fn relay_batch_forwarding_records_errors_and_increments_counters() {
    let store = store();
    let raw = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0];
    let decoded = crate::gsmtap::parse(&raw).unwrap();
    store
        .record(from_decoded(
            "RX",
            Mode::Relay,
            "127.0.0.1:1234".into(),
            &raw,
            &decoded,
        ))
        .await;
    let id = store.list().await[0].id;
    let config = config(
        Mode::Relay,
        "127.0.0.1:0".parse().unwrap(),
        Some("255.255.255.255:4729".into()),
    );
    let router =
        super::super::network::build_router(config, store.clone(), Arc::new(socket().await));
    let request_body = serde_json::json!({ "packetIds": [id] });
    let response = router
        .oneshot(
            axum::http::Request::post("/api/packets/forward")
                .header("content-type", "application/json")
                .body(axum::body::Body::from(request_body.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), axum::http::StatusCode::OK);
    let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
    let json: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert!(json["results"][0]["status"]
        .as_str()
        .unwrap()
        .starts_with("error:"));
    assert_eq!(store.counters().snapshot().forward_failed, 1);
    assert_eq!(store.counters().snapshot().forward_sent, 0);
    assert!(store
        .get(id)
        .await
        .unwrap()
        .forward_status
        .as_deref()
        .unwrap()
        .starts_with("error:"));
}

#[tokio::test]
async fn modify_send_failure_is_recorded() {
    let store = store();
    let raw = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0];
    let decoded = crate::gsmtap::parse(&raw).unwrap();
    store
        .record(from_decoded(
            "RX",
            Mode::Listen,
            "127.0.0.1:1234".into(),
            &raw,
            &decoded,
        ))
        .await;
    let config = config(
        Mode::Modify,
        "127.0.0.1:0".parse().unwrap(),
        Some("255.255.255.255:4729".into()),
    );
    let router =
        super::super::network::build_router(config, store.clone(), Arc::new(socket().await));
    let id = store.list().await[0].id;
    let response = router
        .oneshot(
            axum::http::Request::post(format!("/api/packets/{id}/replay"))
                .body(axum::body::Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), axum::http::StatusCode::BAD_GATEWAY);
    wait_for_records(&store, 2).await;
    assert!(store.list().await[1]
        .forward_status
        .as_deref()
        .is_some_and(|status| status.starts_with("error:")));
}
