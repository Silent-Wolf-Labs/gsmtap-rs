use std::net::SocketAddr;

use clap::{Parser, ValueEnum};

#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum, serde::Serialize)]
#[serde(rename_all = "lowercase")]
pub enum Mode {
    Listen,
    Relay,
    Modify,
}

#[derive(Debug, Clone, Parser)]
#[command(name = "gsmtap-workbench", about = "Live GSMTAP web workbench")]
pub struct ConfigArgs {
    #[arg(long, env = "GSMTAP_MODE", default_value = "listen")]
    pub mode: Mode,
    #[arg(long, env = "GSMTAP_LISTEN", default_value = "0.0.0.0:4729")]
    pub gsmtap_listen: SocketAddr,
    #[arg(long, alias = "gsmtap-target", env = "GSMTAP_FORWARD")]
    pub gsmtap_forward: Option<String>,
    #[arg(long, env = "HTTP_LISTEN", default_value = "127.0.0.1:8080")]
    pub http_listen: SocketAddr,
    #[arg(long, env = "PACKET_HISTORY_CAPACITY", default_value_t = 10_000)]
    pub history_capacity: usize,
    #[arg(long, env = "PACKET_INGRESS_CAPACITY", default_value_t = 1_024)]
    pub ingress_capacity: usize,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub mode: Mode,
    pub gsmtap_listen: SocketAddr,
    pub gsmtap_forward: Option<String>,
    pub http_listen: SocketAddr,
    pub history_capacity: usize,
    pub ingress_capacity: usize,
}

impl Config {
    pub fn from_args() -> Result<Self, String> {
        Self::try_from(ConfigArgs::parse())
    }

    pub fn from_values(
        mode: Mode,
        gsmtap_listen: SocketAddr,
        gsmtap_forward: Option<String>,
        http_listen: SocketAddr,
        history_capacity: usize,
        ingress_capacity: usize,
    ) -> Result<Self, String> {
        Self::build(
            mode,
            gsmtap_listen,
            gsmtap_forward,
            http_listen,
            history_capacity,
            ingress_capacity,
        )
    }

    fn build(
        mode: Mode,
        gsmtap_listen: SocketAddr,
        gsmtap_forward: Option<String>,
        http_listen: SocketAddr,
        history_capacity: usize,
        ingress_capacity: usize,
    ) -> Result<Self, String> {
        if !gsmtap_listen.is_ipv4() || !http_listen.is_ipv4() {
            return Err("the workbench currently supports IPv4 listen addresses only".into());
        }
        let gsmtap_forward = gsmtap_forward.map(|value| value.trim().to_owned());
        if matches!(mode, Mode::Relay | Mode::Modify)
            && gsmtap_forward.as_deref().is_none_or(str::is_empty)
        {
            return Err(format!("--gsmtap-forward is required in {:?} mode", mode));
        }
        if let Some(destination) = gsmtap_forward.as_deref() {
            if destination
                .parse::<SocketAddr>()
                .is_ok_and(|address| address.is_ipv6())
            {
                return Err("the workbench currently supports IPv4 forwarding only".into());
            }
        }
        Ok(Self {
            mode,
            gsmtap_listen,
            gsmtap_forward,
            http_listen,
            history_capacity: history_capacity.max(1),
            ingress_capacity: ingress_capacity.max(1),
        })
    }
}

impl TryFrom<ConfigArgs> for Config {
    type Error = String;

    fn try_from(args: ConfigArgs) -> Result<Self, Self::Error> {
        Self::from_values(
            args.mode,
            args.gsmtap_listen,
            args.gsmtap_forward,
            args.http_listen,
            args.history_capacity,
            args.ingress_capacity,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::{Config, Mode};

    #[test]
    fn from_values_accepts_hostname_forward_endpoint() {
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
    fn all_constructors_enforce_forward_and_ipv4_invariants() {
        assert!(Config::from_values(
            Mode::Relay,
            "127.0.0.1:4729".parse().unwrap(),
            None,
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .is_err());
        assert!(Config::from_values(
            Mode::Listen,
            "[::1]:4729".parse().unwrap(),
            None,
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .is_err());
        assert!(Config::from_values(
            Mode::Relay,
            "127.0.0.1:4729".parse().unwrap(),
            Some("[::1]:9000".into()),
            "127.0.0.1:8080".parse().unwrap(),
            8,
            16,
        )
        .is_err());
    }
}
