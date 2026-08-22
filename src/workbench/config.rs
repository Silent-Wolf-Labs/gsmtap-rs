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
    pub gsmtap_forward: Option<SocketAddr>,
    #[arg(long, env = "HTTP_LISTEN", default_value = "0.0.0.0:8080")]
    pub http_listen: SocketAddr,
    #[arg(long, env = "PACKET_HISTORY_CAPACITY", default_value_t = 256)]
    pub history_capacity: usize,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub mode: Mode,
    pub gsmtap_listen: SocketAddr,
    pub gsmtap_forward: Option<SocketAddr>,
    pub http_listen: SocketAddr,
    pub history_capacity: usize,
}

impl Config {
    pub fn from_args() -> Result<Self, String> {
        Self::try_from(ConfigArgs::parse())
    }

    pub fn from_values(
        mode: Mode,
        gsmtap_listen: SocketAddr,
        gsmtap_forward: Option<SocketAddr>,
        http_listen: SocketAddr,
        history_capacity: usize,
    ) -> Self {
        Self {
            gsmtap_listen,
            mode,
            gsmtap_forward,
            http_listen,
            history_capacity,
        }
    }
}

impl TryFrom<ConfigArgs> for Config {
    type Error = String;

    fn try_from(args: ConfigArgs) -> Result<Self, Self::Error> {
        if matches!(args.mode, Mode::Relay | Mode::Modify) && args.gsmtap_forward.is_none() {
            return Err(format!(
                "--gsmtap-forward is required in {:?} mode",
                args.mode
            ));
        }
        Ok(Self {
            mode: args.mode,
            gsmtap_listen: args.gsmtap_listen,
            gsmtap_forward: args.gsmtap_forward,
            http_listen: args.http_listen,
            history_capacity: args.history_capacity.max(1),
        })
    }
}
