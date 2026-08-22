use std::net::SocketAddr;

use clap::Parser;

#[derive(Debug, Clone, Parser)]
#[command(name = "gsmtap-workbench", about = "Live GSMTAP web workbench")]
pub struct ConfigArgs {
    #[arg(long, env = "GSMTAP_LISTEN", default_value = "0.0.0.0:4729")]
    pub gsmtap_listen: SocketAddr,
    #[arg(long, env = "GSMTAP_TARGET", default_value = "127.0.0.1:4729")]
    pub gsmtap_target: SocketAddr,
    #[arg(long, env = "HTTP_LISTEN", default_value = "0.0.0.0:8080")]
    pub http_listen: SocketAddr,
    #[arg(long, env = "PACKET_HISTORY_CAPACITY", default_value_t = 256)]
    pub history_capacity: usize,
}

#[derive(Debug, Clone)]
pub struct Config {
    pub gsmtap_listen: SocketAddr,
    pub gsmtap_target: SocketAddr,
    pub http_listen: SocketAddr,
    pub history_capacity: usize,
}

impl Config {
    pub fn from_args() -> Self {
        ConfigArgs::parse().into()
    }

    pub fn from_values(
        gsmtap_listen: SocketAddr,
        gsmtap_target: SocketAddr,
        http_listen: SocketAddr,
        history_capacity: usize,
    ) -> Self {
        Self {
            gsmtap_listen,
            gsmtap_target,
            http_listen,
            history_capacity,
        }
    }
}

impl From<ConfigArgs> for Config {
    fn from(args: ConfigArgs) -> Self {
        Self {
            gsmtap_listen: args.gsmtap_listen,
            gsmtap_target: args.gsmtap_target,
            http_listen: args.http_listen,
            history_capacity: args.history_capacity.max(1),
        }
    }
}
