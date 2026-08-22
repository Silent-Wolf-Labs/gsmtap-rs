use std::sync::Arc;

use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use gsmtap_rs::workbench::{
    config::Config,
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
        "127.0.0.1:0".parse().unwrap(),
        target_addr,
        "127.0.0.1:0".parse().unwrap(),
        8,
    );
    let router = build_router(config, store.clone(), sender);
    let receive_task = tokio::spawn(receive_loop(receiver, store.clone()));

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
