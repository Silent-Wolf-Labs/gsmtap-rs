use super::*;

#[tokio::test]
async fn base64_endpoint_matches_direct_c_fixtures_in_all_modes() {
    let fixtures: Vec<Value> = serde_json::from_str(include_str!(
        "../../../tests/vectors/base64/base64_buffer_vectors.json"
    ))
    .unwrap();
    assert_eq!(fixtures.len(), 24);
    for mode in [Mode::Listen, Mode::Relay, Mode::Modify] {
        let (router, store) = router(mode).await;
        for fixture in &fixtures {
            let present = fixture["dst_present"].as_bool().unwrap();
            let capacity = fixture["dst_len"].as_u64().unwrap();
            let payload = json!({"operation":fixture["operation"], "source":{"encoding":"hex","data":fixture["src_hex"]},
                "destination":if present {json!({"capacity":capacity,"fillByte":165})} else {Value::Null},
                "options":{"sizeProbe":!present,"initialOutputLength":0xdecafbadu32}});
            let (status, result) = post_to(&router, "/api/conversions/base64", payload).await;
            assert_eq!(status, StatusCode::OK, "{}", fixture["case"]);
            assert_eq!(result["returnValue"], fixture["return_code"]);
            assert_eq!(result["outputLength"], fixture["olen"]);
            assert_eq!(result["success"], fixture["return_code"] == 0);
            let final_bytes = parse_hex(result["finalDestinationHex"].as_str().unwrap()).unwrap();
            assert_eq!(
                final_bytes,
                parse_hex(fixture["dst_after_hex"].as_str().unwrap()).unwrap(),
                "{}",
                fixture["case"]
            );
            assert_eq!(
                parse_hex(result["initialDestinationHex"].as_str().unwrap()).unwrap(),
                vec![165; if present { capacity as usize } else { 0 }]
            );
            assert_eq!(result["sizeProbe"], !present);
            if result["success"] == true && present {
                let source = parse_hex(fixture["src_hex"].as_str().unwrap()).unwrap();
                let written = if fixture["operation"] == "decode"
                    && source
                        .iter()
                        .all(|byte| matches!(byte, b' ' | b'\r' | b'\n'))
                {
                    0
                } else {
                    fixture["olen"].as_u64().unwrap() as usize
                };
                assert_eq!(
                    parse_hex(result["outputHex"].as_str().unwrap()).unwrap(),
                    final_bytes[..written]
                );
            } else {
                assert!(result["outputHex"].is_null());
            }
        }
        assert!(store.list().await.is_empty());
    }
}

#[tokio::test]
async fn base64_rejects_invalid_requests_and_preserves_initialized_suffixes() {
    let (router, _) = router(Mode::Listen).await;
    let valid = json!({"operation":"encode","source":{"encoding":"text","data":"foo"},"destination":{"capacity":7,"initialHex":"11 22 33 44 55 66 77"},"options":{"initialOutputLength":99}});
    let (status, result) = post_to(&router, "/api/conversions/base64", valid.clone()).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(result["outputText"], "Zm9v");
    assert_eq!(
        parse_hex(result["finalDestinationHex"].as_str().unwrap()).unwrap(),
        [0x5a, 0x6d, 0x39, 0x76, 0, 0x66, 0x77]
    );
    let mut invalids = Vec::new();
    for (key, value) in [
        ("operation", json!("other")),
        ("destination", Value::Null),
        ("options", json!({"sizeProbe":true})),
        ("options", json!({"initialOutputLength":4294967296u64})),
        ("options", json!({"extra":1})),
        ("source", json!({"encoding":"bits","data":"1"})),
    ] {
        let mut request = valid.clone();
        request[key] = value;
        invalids.push(request);
    }
    let mut request = valid.clone();
    request["destination"]["initialHex"] = json!("AA");
    invalids.push(request);
    for request in invalids {
        let (status, _) = post_to(&router, "/api/conversions/base64", request).await;
        assert!(status.is_client_error());
    }
    let mut request = valid;
    request["destination"] = json!({"capacity":65537});
    assert_eq!(
        post_to(&router, "/api/conversions/base64", request).await.0,
        StatusCode::PAYLOAD_TOO_LARGE
    );
    let (status,result)=post_to(&router,"/api/conversions/base64",json!({"operation":"decode","source":{"encoding":"text","data":"/w=="},"destination":{"capacity":1},"options":{}})).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(result["outputHex"], "FF");
    assert!(result["outputText"].is_null());
}
