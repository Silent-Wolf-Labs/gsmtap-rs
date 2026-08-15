use std::collections::BTreeSet;
use std::fs;
use std::path::Path;

const LIBOSMOCORE_COMMIT: &str = "950430e829a3dc1d162aa241bc0505745c5a7311";
const EXPECTED_CASES: &[&str] = &[
    "gsmtap_makemsg_um_wrapper",
    "gsmtap_sim_atr",
    "gsmtap_um_uplink_pcs_boundary",
    "gsmtap_v2_basic_header",
];

#[test]
fn loads_every_c_generated_gsmtap_reference_vector() {
    let vector_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/vectors");
    let mut observed_cases = BTreeSet::new();

    for entry in fs::read_dir(vector_dir).expect("the committed vector directory must be readable")
    {
        let path = entry
            .expect("vector directory entry must be readable")
            .path();
        if path.extension().and_then(|extension| extension.to_str()) != Some("json") {
            continue;
        }

        let contents = fs::read_to_string(&path).expect("committed vector must be readable");
        let vector: serde_json::Value =
            serde_json::from_str(&contents).expect("committed vector must be valid JSON");
        let case_name = vector["case"]
            .as_str()
            .expect("the C-generated vector must contain case");

        assert!(
            observed_cases.insert(case_name.to_owned()),
            "duplicate vector case identifier: {case_name}"
        );
        assert!(EXPECTED_CASES.contains(&case_name));
        assert!(
            matches!(
                vector["api"].as_str(),
                Some("gsmtap_makemsg_ex" | "gsmtap_makemsg")
            ),
            "vector {case_name} must record its C API"
        );
        assert_eq!(
            vector["libosmocore_commit"].as_str(),
            Some(LIBOSMOCORE_COMMIT),
            "vector {case_name} has unexpected reference provenance"
        );
        assert!(
            vector["input"].is_object(),
            "vector {case_name} must record its harness input"
        );
        assert_eq!(vector["return_code"].as_i64(), Some(0));
        assert_eq!(vector["message_created"].as_bool(), Some(true));

        let encoded_hex = vector["encoded_hex"]
            .as_str()
            .expect("the C-generated vector must contain encoded_hex");
        let length = vector["length"]
            .as_u64()
            .expect("the C-generated vector must contain length");
        assert_eq!(encoded_hex.len(), length as usize * 2);
        assert!(encoded_hex.bytes().all(|byte| byte.is_ascii_hexdigit()));
    }

    assert_eq!(
        observed_cases,
        EXPECTED_CASES.iter().map(ToString::to_string).collect(),
        "changing the vector case set is a fixture-contract change"
    );
}
