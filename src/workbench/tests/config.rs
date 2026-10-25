use super::super::config::{Config, ConfigArgs, Mode};

#[test]
fn relay_and_modify_require_a_forward_endpoint() {
    for mode in [Mode::Relay, Mode::Modify] {
        let args = ConfigArgs {
            mode,
            gsmtap_listen: "127.0.0.1:4729".parse().unwrap(),
            gsmtap_forward: None,
            http_listen: "127.0.0.1:8080".parse().unwrap(),
            history_capacity: 8,
            ingress_capacity: 16,
            reference_vectors: None,
        };
        assert!(Config::try_from(args).is_err());
    }
}

#[test]
fn hostname_forward_targets_are_accepted() {
    let config = Config::from_values(
        Mode::Relay,
        "127.0.0.1:4729".parse().unwrap(),
        Some("traffic:9000".into()),
        "127.0.0.1:8080".parse().unwrap(),
        8,
        16,
    )
    .unwrap();
    assert_eq!(config.gsmtap_forward.as_deref(), Some("traffic:9000"));
}

#[test]
fn ipv6_addresses_are_rejected_by_all_constructor_paths() {
    assert!(
        Config::from_values(
            Mode::Listen,
            "[::1]:4729".parse().unwrap(),
            None,
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .is_err()
    );
    assert!(
        Config::from_values(
            Mode::Relay,
            "127.0.0.1:4729".parse().unwrap(),
            Some("[::1]:9000".into()),
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .is_err()
    );
}

#[test]
fn reference_vectors_directory_is_passed_through() {
    let args = ConfigArgs {
        mode: Mode::Listen,
        gsmtap_listen: "127.0.0.1:4729".parse().unwrap(),
        gsmtap_forward: None,
        http_listen: "127.0.0.1:8080".parse().unwrap(),
        history_capacity: 8,
        ingress_capacity: 16,
        reference_vectors: Some("/vectors".into()),
    };
    let config = Config::try_from(args).unwrap();
    assert_eq!(
        config.reference_vectors.as_deref(),
        Some(std::path::Path::new("/vectors"))
    );
}
