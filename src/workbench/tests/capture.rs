use super::{config, socket, store, wait_for_records};
use crate::workbench::{
    capture::CaptureControl,
    config::Mode,
    network::{build_router_with_capture, receive_loop_gated, receive_loop_gated_with_capacity},
};
use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use std::sync::Arc;
use tokio::{
    sync::Notify,
    time::{sleep, timeout, Duration},
};
use tower::ServiceExt;

const PACKET: [u8; 17] = [2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xca];

#[tokio::test]
async fn paused_capture_counts_traffic_without_history_or_sse() {
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = store();
    let control = CaptureControl::default();
    control.set_paused(true);
    let mut events = store.subscribe();
    let task = tokio::spawn(receive_loop_gated(
        receiver,
        store.clone(),
        Mode::Listen,
        Arc::new(socket().await),
        control,
        Arc::new(Notify::new()),
    ));
    source.send_to(&PACKET, receiver_addr).await.unwrap();
    timeout(Duration::from_secs(1), async {
        while store.counters().snapshot().received < 1 {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    assert_eq!(store.counters().snapshot().capture_skipped, 1);
    assert!(store.list().await.is_empty());
    assert!(timeout(Duration::from_millis(30), events.recv())
        .await
        .is_err());
    task.abort();
}

#[tokio::test]
async fn paused_relay_does_not_forward_and_counts_skipped() {
    let target = socket().await;
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = store();
    let control = CaptureControl::default();
    control.set_paused(true);
    let gate = Arc::new(Notify::new());
    let task = tokio::spawn(receive_loop_gated_with_capacity(
        receiver,
        store.clone(),
        Mode::Relay,
        Some(target.local_addr().unwrap().to_string()),
        control,
        gate.clone(),
        8,
    ));
    source.send_to(&PACKET, receiver_addr).await.unwrap();
    timeout(Duration::from_secs(1), async {
        while store.counters().snapshot().received < 1 {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    gate.notify_one();
    let mut forwarded = [0u8; 64];
    assert!(
        timeout(Duration::from_millis(50), target.recv_from(&mut forwarded))
            .await
            .is_err()
    );
    assert!(store.list().await.is_empty());
    assert_eq!(store.counters().snapshot().forward_sent, 0);
    assert_eq!(store.counters().snapshot().capture_skipped, 1);
    task.abort();
}

#[tokio::test]
async fn paused_queue_drop_counts_both_skip_and_ingress_drop() {
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = store();
    let control = CaptureControl::default();
    control.set_paused(true);
    let gate = Arc::new(Notify::new());
    let task = tokio::spawn(receive_loop_gated_with_capacity(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        control,
        gate,
        1,
    ));
    for _ in 0..3 {
        source.send_to(&PACKET, receiver_addr).await.unwrap();
    }
    timeout(Duration::from_secs(1), async {
        while store.counters().snapshot().received < 3 {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    assert_eq!(store.counters().snapshot().capture_skipped, 3);
    assert!(store.counters().snapshot().ingress_dropped >= 1);
    task.abort();
}

#[tokio::test]
async fn receive_time_snapshot_wins_when_capture_resumes_before_processing() {
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = store();
    let control = CaptureControl::default();
    let gate = Arc::new(Notify::new());
    let task = tokio::spawn(receive_loop_gated(
        receiver,
        store.clone(),
        Mode::Listen,
        Arc::new(socket().await),
        control.clone(),
        gate.clone(),
    ));
    control.set_paused(true);
    source.send_to(&PACKET, receiver_addr).await.unwrap();
    timeout(Duration::from_secs(1), async {
        while store.counters().snapshot().received < 1 {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    control.set_paused(false);
    gate.notify_one();
    sleep(Duration::from_millis(20)).await;
    assert!(store.list().await.is_empty());
    source.send_to(&PACKET, receiver_addr).await.unwrap();
    gate.notify_one();
    wait_for_records(&store, 1).await;
    assert_eq!(store.counters().snapshot().received, 2);
    task.abort();
}

#[tokio::test]
async fn capture_endpoint_is_idempotent_and_status_reflects_it() {
    let control = CaptureControl::default();
    let forward = socket().await;
    let router = build_router_with_capture(
        config(
            Mode::Relay,
            "127.0.0.1:4729".parse().unwrap(),
            Some(forward.local_addr().unwrap().to_string()),
        ),
        store(),
        Arc::new(socket().await),
        control,
    );
    for paused in [true, true, false] {
        let response = router
            .clone()
            .oneshot(
                Request::put("/api/capture")
                    .header("content-type", "application/json")
                    .body(Body::from(
                        serde_json::json!({"paused": paused}).to_string(),
                    ))
                    .unwrap(),
            )
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        let body = hyper::body::to_bytes(response.into_body()).await.unwrap();
        let json: serde_json::Value = serde_json::from_slice(&body).unwrap();
        assert_eq!(json["capturePaused"], paused);
    }
}

#[tokio::test]
async fn paused_capture_does_not_suppress_explicit_tx_records() {
    let target = socket().await;
    let store = store();
    let decoded = crate::gsmtap::parse(&PACKET).unwrap();
    store
        .record(crate::workbench::dto::from_decoded(
            "RX",
            Mode::Modify,
            "peer".into(),
            &PACKET,
            &decoded,
        ))
        .await;
    let control = CaptureControl::default();
    control.set_paused(true);
    let router = build_router_with_capture(
        config(
            Mode::Modify,
            "127.0.0.1:4729".parse().unwrap(),
            Some(target.local_addr().unwrap().to_string()),
        ),
        store.clone(),
        Arc::new(socket().await),
        control,
    );
    let response = router
        .oneshot(
            Request::post("/api/packets/1/replay")
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let mut bytes = [0u8; 64];
    let (length, _) = target.recv_from(&mut bytes).await.unwrap();
    assert_eq!(&bytes[..length], &PACKET);
    assert_eq!(store.list().await.len(), 2);
    assert_eq!(store.list().await[1].direction, "TX");
}
