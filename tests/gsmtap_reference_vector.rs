use serde_json::Value;

const BASIC_HEADER_VECTOR: &str = include_str!("vectors/gsmtap_v2_basic_header.json");
const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";

#[test]
fn loads_the_c_generated_gsmtap_reference_vector() {
    let vector: Value = serde_json::from_str(BASIC_HEADER_VECTOR)
        .expect("the committed libosmocore reference vector must be valid JSON");

    assert_eq!(vector["case"].as_str(), Some("gsmtap_v2_basic_header"));
    assert_eq!(
        vector["libosmocore_commit"].as_str(),
        Some(LIBOSMOCORE_COMMIT)
    );
    assert_eq!(vector["return_code"].as_i64(), Some(0));

    let encoded_hex = vector["encoded_hex"]
        .as_str()
        .expect("the C-generated vector must contain encoded_hex");
    let length = vector["length"]
        .as_u64()
        .expect("the C-generated vector must contain length");
    assert_eq!(encoded_hex.len(), length as usize * 2);
    assert!(encoded_hex.bytes().all(|byte| byte.is_ascii_hexdigit()));
}
