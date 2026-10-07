use axum::{
    body::Body,
    http::{Request, StatusCode},
    Router,
};
use serde_json::{json, Value};
use std::sync::Arc;
use tower::ServiceExt;

use super::super::{
    config::{Config, Mode},
    conversions::MAX_REQUEST_BYTES,
    dto::parse_hex,
    history::PacketStore,
    network::build_router,
};

async fn router(mode: Mode) -> (Router, Arc<PacketStore>) {
    let store = Arc::new(PacketStore::new(8));
    let config = Config::from_values(
        mode,
        "127.0.0.1:4729".parse().unwrap(),
        (mode != Mode::Listen).then(|| "127.0.0.1:9000".to_owned()),
        "127.0.0.1:8080".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    let socket = Arc::new(tokio::net::UdpSocket::bind("127.0.0.1:0").await.unwrap());
    (build_router(config, store.clone(), socket), store)
}

async fn post(router: &Router, request: Value) -> (StatusCode, Value) {
    post_to(router, "/api/conversions/hexparse", request).await
}

async fn post_to(router: &Router, path: &str, request: Value) -> (StatusCode, Value) {
    let response = router
        .clone()
        .oneshot(
            Request::post(path)
                .header("content-type", "application/json")
                .body(Body::from(request.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    let status = response.status();
    let bytes = hyper::body::to_bytes(response.into_body()).await.unwrap();
    (
        status,
        serde_json::from_slice(&bytes).unwrap_or(Value::Null),
    )
}

fn request() -> Value {
    json!({"operation":"parse", "source":{"encoding":"text", "data":"CA FE"},
        "destination":{"capacity":4,"fillByte":170,"initialHex":""},"options":{}})
}

#[tokio::test]
async fn hexparse_endpoint_matches_all_c_fixtures_in_every_mode() {
    let fixtures: Vec<Value> = serde_json::from_str(include_str!(
        "../../../tests/vectors/hexparse/hexparse_buffer_vectors.json"
    ))
    .unwrap();
    for mode in [Mode::Listen, Mode::Relay, Mode::Modify] {
        let (router, store) = router(mode).await;
        for fixture in &fixtures {
            let (status, result) = post(&router, json!({
                "operation":"parse", "source":{"encoding":"hex","data":fixture["src_hex"]},
                "destination":{"capacity":fixture["dst_len"],"initialHex":fixture["dst_before_hex"]}, "options":{}
            })).await;
            assert_eq!(status, StatusCode::OK, "{}", fixture["case"]);
            let expected_return = fixture["return_code"].as_i64().unwrap();
            assert_eq!(
                result["success"],
                expected_return >= 0,
                "{}",
                fixture["case"]
            );
            assert_eq!(
                result["returnValue"],
                if expected_return >= 0 {
                    json!(expected_return)
                } else {
                    Value::Null
                }
            );
            for (actual, expected) in [
                ("initialDestinationHex", "dst_before_hex"),
                ("finalDestinationHex", "dst_after_hex"),
            ] {
                assert_eq!(
                    parse_hex(result[actual].as_str().unwrap()).unwrap(),
                    parse_hex(fixture[expected].as_str().unwrap()).unwrap(),
                    "{} {actual}",
                    fixture["case"]
                );
            }
            if expected_return < 0 {
                assert!(result["outputHex"].is_null());
                assert_eq!(result["error"]["code"], "invalidInput");
            } else {
                let final_bytes = parse_hex(fixture["dst_after_hex"].as_str().unwrap()).unwrap();
                assert_eq!(
                    parse_hex(result["outputHex"].as_str().unwrap()).unwrap(),
                    final_bytes[..expected_return as usize]
                );
            }
        }
        assert!(store.list().await.is_empty());
        assert_eq!(store.counters().snapshot().received, 0);
    }
}
#[tokio::test]
async fn hexparse_text_and_byte_sources_produce_identical_results() {
    let (router, _) = router(Mode::Listen).await;
    let (_, text) = post(&router, request()).await;
    let mut byte_request = request();
    byte_request["source"] = json!({"encoding":"hex","data":"43 41 20 46 45"});
    let (_, bytes) = post(&router, byte_request).await;
    assert_eq!(text, bytes);
}
#[tokio::test]
async fn hexparse_rejects_invalid_structure_and_bounds_before_conversion() {
    let (router, _) = router(Mode::Listen).await;
    for (field, value, expected) in [
        ("operation", json!("encode"), StatusCode::BAD_REQUEST),
        ("options", json!({"ignored":true}), StatusCode::BAD_REQUEST),
        (
            "source",
            json!({"encoding":"bits","data":"1"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "source",
            json!({"encoding":"hex","data":"GG"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "source",
            json!({"encoding":"text","data":"a".repeat(196609)}),
            StatusCode::PAYLOAD_TOO_LARGE,
        ),
        (
            "destination",
            json!({"capacity":65537}),
            StatusCode::PAYLOAD_TOO_LARGE,
        ),
        (
            "destination",
            json!({"capacity":4,"initialHex":"AA"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "destination",
            json!({"capacity":4,"initialHex":"GG"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "destination",
            json!({"capacity":-1}),
            StatusCode::UNPROCESSABLE_ENTITY,
        ),
        (
            "destination",
            json!({"capacity":4,"fillByte":256}),
            StatusCode::UNPROCESSABLE_ENTITY,
        ),
        ("destination", Value::Null, StatusCode::UNPROCESSABLE_ENTITY),
        (
            "source",
            json!({"encoding":"text","data":"CA","unknown":0}),
            StatusCode::UNPROCESSABLE_ENTITY,
        ),
    ] {
        let mut payload = request();
        payload[field] = value;
        assert_eq!(post(&router, payload).await.0, expected, "{field}");
    }
    let body = format!("{}{}", request(), " ".repeat(MAX_REQUEST_BYTES));
    let response = router
        .oneshot(
            Request::post("/api/conversions/hexparse")
                .header("content-type", "application/json")
                .body(Body::from(body))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::PAYLOAD_TOO_LARGE);
}
