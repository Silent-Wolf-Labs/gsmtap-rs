use super::super::{config::Mode, dto::from_decoded, network::receive_loop};
use super::{config, socket, store, wait_for_records};
use std::sync::Arc;
use tower::ServiceExt;

#[tokio::test]
async fn relay_failure_is_recorded_and_does_not_stop_receiving() {
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
    assert!(store.list().await.iter().all(|packet| packet
        .forward_status
        .as_deref()
        .is_some_and(|status| status.starts_with("error:"))));
    assert!(!task.is_finished());
    task.abort();
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
