use super::super::{
    config::Mode,
    history::PacketStore,
    network::{build_router, receive_loop},
    reference::ReferenceVectors,
};
use super::{config, socket, wait_for_records};
use axum::{
    body::Body,
    http::{Request, StatusCode},
};
use http_body_util::BodyExt;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tower::ServiceExt;

fn vectors_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/vectors")
}

/// Each committed vector, as `(case, bytes)`.
fn committed_vectors() -> Vec<(String, Vec<u8>)> {
    let mut vectors = Vec::new();
    for entry in std::fs::read_dir(vectors_dir()).unwrap() {
        let path = entry.unwrap().path();
        if path
            .extension()
            .is_some_and(|extension| extension == "json")
        {
            let vector: serde_json::Value =
                serde_json::from_str(&std::fs::read_to_string(&path).unwrap()).unwrap();
            let hex = vector["encoded_hex"].as_str().unwrap();
            let bytes = (0..hex.len())
                .step_by(2)
                .map(|index| u8::from_str_radix(&hex[index..index + 2], 16).unwrap())
                .collect();
            vectors.push((vector["case"].as_str().unwrap().to_owned(), bytes));
        }
    }
    vectors
}

/// A fresh, empty directory for one test.
fn scratch_dir(name: &str) -> PathBuf {
    let directory =
        std::env::temp_dir().join(format!("gsmtap-rs-reference-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&directory);
    std::fs::create_dir_all(&directory).unwrap();
    directory
}

#[test]
fn loads_every_committed_gsmtap_vector() {
    let references = ReferenceVectors::load(&vectors_dir()).unwrap();
    let vectors = committed_vectors();
    assert_eq!(vectors.len(), 7);
    assert_eq!(references.len(), vectors.len());
    for (case, bytes) in &vectors {
        assert_eq!(references.case(bytes), Some(case.as_str()));
    }
}

#[test]
fn unrecorded_and_truncated_packets_do_not_match() {
    let references = ReferenceVectors::load(&vectors_dir()).unwrap();
    let (_, bytes) = &committed_vectors()[0];
    assert_eq!(references.case(&bytes[..bytes.len() - 1]), None);
    let mut changed = bytes.clone();
    *changed.last_mut().unwrap() ^= 1;
    assert_eq!(references.case(&changed), None);
    assert_eq!(references.case(&[0xde, 0xad, 0xbe, 0xef]), None);
}

#[test]
fn first_case_wins_for_duplicate_packets() {
    let directory = scratch_dir("duplicate");
    for case in ["b_case", "a_case"] {
        std::fs::write(
            directory.join(format!("{case}.json")),
            format!(r#"{{"case": "{case}", "encoded_hex": "020401"}}"#),
        )
        .unwrap();
    }
    std::fs::write(directory.join("notes.txt"), "ignored").unwrap();
    let references = ReferenceVectors::load(&directory).unwrap();
    assert_eq!(references.len(), 1);
    assert_eq!(references.case(&[2, 4, 1]), Some("a_case"));
    std::fs::remove_dir_all(directory).unwrap();
}

#[test]
fn rejects_missing_empty_and_invalid_directories() {
    let missing = std::env::temp_dir().join("gsmtap-rs-reference-does-not-exist");
    assert!(ReferenceVectors::load(&missing).is_err());

    let empty = scratch_dir("empty");
    let error = ReferenceVectors::load(&empty).unwrap_err();
    assert!(error.contains("no reference vectors"), "{error}");

    for (name, text) in [
        ("not-json", "not json"),
        ("no-hex", r#"{"case": "x"}"#),
        ("bad-hex", r#"{"case": "x", "encoded_hex": "0g"}"#),
    ] {
        let directory = scratch_dir(name);
        std::fs::write(directory.join("vector.json"), text).unwrap();
        let error = ReferenceVectors::load(&directory).unwrap_err();
        assert!(error.contains("vector.json"), "{name}: {error}");
        std::fs::remove_dir_all(directory).unwrap();
    }
    std::fs::remove_dir_all(empty).unwrap();
}

#[tokio::test]
async fn received_packets_are_tagged_and_counted() {
    let store = Arc::new(PacketStore::with_references(
        8,
        ReferenceVectors::load(&vectors_dir()).unwrap(),
    ));
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        Arc::new(socket().await),
    ));
    let (case, bytes) = committed_vectors().remove(0);
    let sender = socket().await;
    sender.send_to(&bytes, receiver_addr).await.unwrap();
    sender
        .send_to(&[0xde, 0xad, 0xbe, 0xef], receiver_addr)
        .await
        .unwrap();
    wait_for_records(&store, 2).await;
    task.abort();

    let records = store.list().await;
    assert_eq!(records[0].reference_case.as_deref(), Some(case.as_str()));
    assert_eq!(records[1].reference_case, None);

    let router = build_router(
        config(Mode::Listen, "127.0.0.1:0".parse().unwrap(), None),
        store,
        Arc::new(socket().await),
    );
    let response = router
        .clone()
        .oneshot(Request::get("/api/status").body(Body::empty()).unwrap())
        .await
        .unwrap();
    assert_eq!(response.status(), StatusCode::OK);
    let body = response.into_body().collect().await.unwrap().to_bytes();
    let status: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(status["referenceVectors"], 7);
    assert_eq!(status["stats"]["referenceMatched"], 1);

    let response = router
        .oneshot(Request::get("/api/packets").body(Body::empty()).unwrap())
        .await
        .unwrap();
    let body = response.into_body().collect().await.unwrap().to_bytes();
    let packets: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(packets[0]["referenceCase"], case.as_str());
    assert_eq!(packets[1]["referenceCase"], serde_json::Value::Null);
}

#[tokio::test]
async fn status_reports_no_vectors_by_default() {
    let router = build_router(
        config(Mode::Listen, "127.0.0.1:0".parse().unwrap(), None),
        Arc::new(PacketStore::new(8)),
        Arc::new(socket().await),
    );
    let response = router
        .oneshot(Request::get("/api/status").body(Body::empty()).unwrap())
        .await
        .unwrap();
    let body = response.into_body().collect().await.unwrap().to_bytes();
    let status: serde_json::Value = serde_json::from_slice(&body).unwrap();
    assert_eq!(status["referenceVectors"], 0);
    assert_eq!(status["stats"]["referenceMatched"], 0);
}
