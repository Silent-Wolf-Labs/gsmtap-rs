use super::*;

#[tokio::test]
async fn bits_endpoint_matches_basic_and_extended_c_fixtures() {
    for fixture_json in [
        include_str!("../../../tests/vectors/bits/bit_packing_vectors.json"),
        include_str!("../../../tests/vectors/bits/bit_packing_ext_vectors.json"),
    ] {
        let fixtures: Vec<Value> = serde_json::from_str(fixture_json).unwrap();
        for mode in [Mode::Listen, Mode::Relay, Mode::Modify] {
            let (router, store) = router(mode).await;
            for (index, fixture) in fixtures.iter().enumerate() {
                // Exhaustive HTTP fixture coverage in Listen; representative
                // batches also ensure conversion availability in other modes.
                if mode != Mode::Listen && index % 1024 != 0 {
                    continue;
                }
                let api = fixture["api"].as_str().unwrap();
                let operation = match api {
                    "osmo_ubit2pbit" => "pack",
                    "osmo_pbit2ubit" => "unpack",
                    "osmo_ubit2pbit_ext" => "pack-ext",
                    "osmo_pbit2ubit_ext" => "unpack-ext",
                    _ => panic!("unexpected bit API"),
                };
                let mut options = json!({"numBits":fixture["num_bits"]});
                if operation.ends_with("-ext") {
                    options["inputOffset"] = fixture["in_ofs"].clone();
                    options["outputOffset"] = fixture["out_ofs"].clone();
                    options["lsbMode"] = json!(fixture["lsb_mode"].as_i64().unwrap() != 0);
                }
                let (status, result) = post_to(&router,"/api/conversions/bits",json!({"operation":operation,
                    "source":{"encoding":"hex","data":fixture["src_hex"]},
                    "destination":{"capacity":fixture["dst_len"],"initialHex":fixture["dst_before_hex"]},"options":options})).await;
                assert_eq!(status, StatusCode::OK, "{}", fixture["case"]);
                assert_eq!(result["success"], true, "{}", fixture["case"]);
                assert_eq!(
                    result["returnValue"], fixture["return_code"],
                    "{}",
                    fixture["case"]
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
                if operation == "unpack-ext" && fixture["num_bits"] == 0 {
                    assert_eq!(result["outputText"], "");
                }
            }
            assert!(store.list().await.is_empty());
        }
    }
}

#[tokio::test]
async fn bits_formats_and_capacity_errors_are_safe() {
    let (router, _) = router(Mode::Listen).await;
    let basic = json!({"operation":"pack","source":{"encoding":"bits","data":"0 1\n1 0"},
        "destination":{"capacity":2,"fillByte":170},"options":{"numBits":4}});
    let (status, bits) = post_to(&router, "/api/conversions/bits", basic.clone()).await;
    assert_eq!(status, StatusCode::OK);
    let mut raw = basic.clone();
    raw["source"] = json!({"encoding":"hex","data":"00 01 01 00"});
    assert_eq!(post_to(&router, "/api/conversions/bits", raw).await.1, bits);
    for (key, value, expected) in [
        ("operation", json!("decode"), StatusCode::BAD_REQUEST),
        (
            "source",
            json!({"encoding":"bits","data":"102"}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"numBits":524289}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"numBits":4,"lsbMode":false}),
            StatusCode::BAD_REQUEST,
        ),
        (
            "options",
            json!({"numBits":4,"unknown":1}),
            StatusCode::BAD_REQUEST,
        ),
        ("options", json!({"numBits":-1}), StatusCode::BAD_REQUEST),
        (
            "destination",
            json!({"capacity":65537}),
            StatusCode::PAYLOAD_TOO_LARGE,
        ),
    ] {
        let mut payload = basic.clone();
        payload[key] = value;
        assert_eq!(
            post_to(&router, "/api/conversions/bits", payload).await.0,
            expected
        );
    }
    for (options, expected) in [
        (json!({"numBits":5}), "inputTooShort"),
        (json!({"numBits":4}), "outputTooShort"),
    ] {
        let mut payload = basic.clone();
        payload["options"] = options;
        payload["destination"] = json!({"capacity":0});
        let (status, result) = post_to(&router, "/api/conversions/bits", payload).await;
        assert_eq!(status, StatusCode::OK);
        assert_eq!(result["error"]["code"], expected);
        assert_eq!(
            result["initialDestinationHex"],
            result["finalDestinationHex"]
        );
    }
    let mut unpack = basic;
    unpack["operation"] = json!("unpack");
    assert_eq!(
        post_to(&router, "/api/conversions/bits", unpack).await.0,
        StatusCode::BAD_REQUEST
    );
}
