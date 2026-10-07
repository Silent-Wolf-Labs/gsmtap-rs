use super::*;

#[tokio::test]
async fn bcd_endpoint_matches_all_c_fixtures_in_every_mode() {
    let fixtures: Vec<Value> = serde_json::from_str(include_str!(
        "../../../tests/vectors/bcd/bcd_conversion_vectors.json"
    ))
    .unwrap();
    for mode in [Mode::Listen, Mode::Relay, Mode::Modify] {
        let (router, store) = router(mode).await;
        for fixture in &fixtures {
            let operation = fixture["api"]
                .as_str()
                .unwrap()
                .strip_prefix("osmo_")
                .unwrap();
            let scalar = matches!(operation, "char2bcd" | "bcd2char");
            let destination = if scalar {
                Value::Null
            } else {
                json!({"capacity":fixture["dst_len"],"initialHex":fixture["dst_before_hex"]})
            };
            let options = if scalar {
                json!({})
            } else {
                json!({"startNibble":fixture["start_nibble"],
                    "endNibble": if fixture["end_nibble"] == -1 { Value::Null } else { fixture["end_nibble"].clone() },
                    "allowHex":fixture["allow_hex"]})
            };
            let (status, result) = post_to(&router, "/api/conversions/bcd", json!({"operation":operation,
                "source":{"encoding":"hex","data":fixture["src_hex"]},"destination":destination,"options":options})).await;
            assert_eq!(status, StatusCode::OK, "{}", fixture["case"]);
            let expected = fixture["return_code"].as_i64().unwrap();
            assert_eq!(result["success"], expected >= 0, "{}", fixture["case"]);
            assert_eq!(
                result["returnValue"],
                if expected >= 0 {
                    json!(expected)
                } else {
                    Value::Null
                },
                "{}",
                fixture["case"]
            );
            if scalar {
                assert_eq!(
                    parse_hex(result["outputHex"].as_str().unwrap()).unwrap(),
                    [expected as u8]
                );
                assert_eq!(result["finalDestinationHex"], "");
            } else {
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
                if operation == "bcd2str" && (expected >= 0 || expected == -22) {
                    let bytes = parse_hex(fixture["dst_after_hex"].as_str().unwrap()).unwrap();
                    let end = bytes.iter().position(|&byte| byte == 0).unwrap();
                    assert_eq!(
                        result["outputText"],
                        String::from_utf8(bytes[..end].to_vec()).unwrap()
                    );
                }
            }
            if expected < 0 {
                assert_eq!(
                    result["error"]["code"],
                    if expected == -22 {
                        "invalidDigit"
                    } else {
                        "noSpace"
                    }
                );
            }
        }
        assert!(store.list().await.is_empty());
    }
}

#[tokio::test]
async fn bcd_rejects_invalid_requests_and_preserves_buffers_on_slice_errors() {
    let (router, _) = router(Mode::Listen).await;
    let basic = json!({"operation":"str2bcd","source":{"encoding":"text","data":"123"},
        "destination":{"capacity":4,"fillByte":170},"options":{"startNibble":0,"endNibble":null,"allowHex":false}});
    for (key, value, expected) in [
        ("operation", json!("parse"), StatusCode::BAD_REQUEST),
        ("destination", Value::Null, StatusCode::BAD_REQUEST),
        (
            "destination",
            json!({"capacity":65537}),
            StatusCode::PAYLOAD_TOO_LARGE,
        ),
        (
            "options",
            json!({"startNibble":-1}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"startNibble":131073}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"startNibble":0,"allowHex":"false"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"startNibble":0,"unknown":0}),
            StatusCode::BAD_REQUEST,
        ),
    ] {
        let mut payload = basic.clone();
        payload[key] = value;
        assert_eq!(
            post_to(&router, "/api/conversions/bcd", payload).await.0,
            expected
        );
    }
    let scalar = json!({"operation":"bcd2char","source":{"encoding":"hex","data":"10"},"destination":null,"options":{}});
    assert_eq!(
        post_to(&router, "/api/conversions/bcd", scalar).await.0,
        StatusCode::BAD_REQUEST
    );
    let mut decode = basic;
    decode["operation"] = json!("bcd2str");
    assert_eq!(
        post_to(&router, "/api/conversions/bcd", decode.clone())
            .await
            .0,
        StatusCode::BAD_REQUEST
    );
    decode["source"] = json!({"encoding":"hex","data":"21"});
    decode["options"] = json!({"startNibble":0,"endNibble":3,"allowHex":false});
    let (status, result) = post_to(&router, "/api/conversions/bcd", decode).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(result["error"]["code"], "outOfRange");
    assert_eq!(
        result["initialDestinationHex"],
        result["finalDestinationHex"]
    );
    assert!(result["outputText"].is_null());
}
