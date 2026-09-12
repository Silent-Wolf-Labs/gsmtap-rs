use std::{io, sync::Arc};

#[cfg(test)]
use std::net::SocketAddr;

use tokio::{
    net::UdpSocket,
    sync::{broadcast, Mutex},
    task::JoinHandle,
};

use super::{
    capture::CaptureControl,
    config::{Config, Mode},
    history::PacketStore,
    network::{inspection_worker_until, udp_receiver_until},
};

struct PipelineHandle {
    shutdown: broadcast::Sender<()>,
    intake: JoinHandle<Result<(), io::Error>>,
    processing: JoinHandle<Result<(), io::Error>>,
}

impl PipelineHandle {
    fn start(
        config: &Config,
        mode: Mode,
        forward: Option<String>,
        receiver: Arc<UdpSocket>,
        store: Arc<PacketStore>,
        capture: CaptureControl,
    ) -> Self {
        let (ingress_tx, ingress_rx) = tokio::sync::mpsc::channel(config.ingress_capacity);
        let (shutdown, _) = broadcast::channel(1);
        let intake = tokio::spawn(udp_receiver_until(
            receiver,
            ingress_tx,
            store.clone(),
            capture,
            shutdown.subscribe(),
        ));
        let processing = tokio::spawn(inspection_worker_until(
            ingress_rx,
            store,
            mode,
            forward,
            None,
            shutdown.subscribe(),
        ));
        Self {
            shutdown,
            intake,
            processing,
        }
    }

    async fn stop(self) -> Result<(), io::Error> {
        let _ = self.shutdown.send(());
        let intake = self
            .intake
            .await
            .map_err(join_error)
            .and_then(|result| result);
        let processing = self
            .processing
            .await
            .map_err(join_error)
            .and_then(|result| result);
        intake.and(processing)
    }
}

pub(crate) trait PipelineFactory: Send + Sync {
    fn preflight(&self, mode: Mode, forward: Option<&str>) -> Result<(), io::Error>;
}

struct DefaultPipelineFactory;

impl PipelineFactory for DefaultPipelineFactory {
    fn preflight(&self, mode: Mode, forward: Option<&str>) -> Result<(), io::Error> {
        if matches!(mode, Mode::Relay | Mode::Modify) && forward.is_none_or(str::is_empty) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                format!("a forward endpoint is required in {mode:?} mode"),
            ));
        }
        if let Some(forward) = forward {
            validate_forward_endpoint(forward)?;
        }
        Ok(())
    }
}

fn validate_forward_endpoint(forward: &str) -> Result<(), io::Error> {
    if let Ok(address) = forward.parse::<std::net::SocketAddr>() {
        if address.is_ipv4() {
            return Ok(());
        }
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "the workbench currently supports IPv4 forwarding only",
        ));
    }
    let Some((host, port)) = forward.rsplit_once(':') else {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "forward address must be an IPv4 address or hostname followed by a port",
        ));
    };
    if host.is_empty() || port.parse::<u16>().is_err() {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "forward address must be an IPv4 address or hostname followed by a port",
        ));
    }
    Ok(())
}

fn join_error(error: tokio::task::JoinError) -> io::Error {
    io::Error::other(format!("pipeline task failed: {error}"))
}

struct RuntimeState {
    mode: Mode,
    forward: Option<String>,
    pipeline: Option<PipelineHandle>,
}

struct RuntimeInner {
    config: Config,
    store: Arc<PacketStore>,
    capture: CaptureControl,
    receiver: Option<Arc<UdpSocket>>,
    pipeline_factory: Arc<dyn PipelineFactory>,
    state: Mutex<RuntimeState>,
    transition: Mutex<()>,
}

#[derive(Clone)]
pub struct RuntimeHandle(Arc<RuntimeInner>);

impl RuntimeHandle {
    pub async fn start(
        config: Config,
        store: Arc<PacketStore>,
        capture: CaptureControl,
    ) -> Result<Self, io::Error> {
        Self::start_with_factory(config, store, capture, Arc::new(DefaultPipelineFactory)).await
    }

    async fn start_with_factory(
        config: Config,
        store: Arc<PacketStore>,
        capture: CaptureControl,
        pipeline_factory: Arc<dyn PipelineFactory>,
    ) -> Result<Self, io::Error> {
        let mode = config.mode;
        let forward = config.gsmtap_forward.clone();
        pipeline_factory.preflight(mode, forward.as_deref())?;
        let receiver = Arc::new(UdpSocket::bind(config.gsmtap_listen).await?);
        let pipeline = PipelineHandle::start(
            &config,
            mode,
            forward.clone(),
            receiver.clone(),
            store.clone(),
            capture.clone(),
        );
        Ok(Self(Arc::new(RuntimeInner {
            config,
            store,
            capture,
            receiver: Some(receiver),
            pipeline_factory,
            state: Mutex::new(RuntimeState {
                mode,
                forward,
                pipeline: Some(pipeline),
            }),
            transition: Mutex::new(()),
        })))
    }

    #[cfg(test)]
    pub(crate) async fn start_with_test_factory(
        config: Config,
        store: Arc<PacketStore>,
        capture: CaptureControl,
        pipeline_factory: Arc<dyn PipelineFactory>,
    ) -> Result<Self, io::Error> {
        Self::start_with_factory(config, store, capture, pipeline_factory).await
    }

    #[cfg(test)]
    pub fn detached(config: Config, store: Arc<PacketStore>, capture: CaptureControl) -> Self {
        let mode = config.mode;
        let forward = config.gsmtap_forward.clone();
        Self(Arc::new(RuntimeInner {
            config,
            store,
            capture,
            receiver: None,
            pipeline_factory: Arc::new(DefaultPipelineFactory),
            state: Mutex::new(RuntimeState {
                mode,
                forward,
                pipeline: None,
            }),
            transition: Mutex::new(()),
        }))
    }

    pub async fn mode(&self) -> Mode {
        self.0.state.lock().await.mode
    }

    pub async fn forward(&self) -> Option<String> {
        self.0.state.lock().await.forward.clone()
    }

    #[cfg(test)]
    pub(crate) fn listen_addr(&self) -> Option<SocketAddr> {
        self.0
            .receiver
            .as_ref()
            .and_then(|socket| socket.local_addr().ok())
    }

    pub async fn change_mode(
        &self,
        mode: Mode,
        requested_forward: Option<String>,
    ) -> Result<(Mode, Option<String>), io::Error> {
        let _transition = self.0.transition.lock().await;
        let (previous_mode, previous_forward, forward) = {
            let state = self.0.state.lock().await;
            let forward = match mode {
                Mode::Listen => None,
                Mode::Relay | Mode::Modify => requested_forward
                    .map(|value| value.trim().to_owned())
                    .filter(|value| !value.is_empty())
                    .or_else(|| state.forward.clone()),
            };
            if state.mode == mode && state.forward == forward {
                return Ok((mode, forward));
            }
            (state.mode, state.forward.clone(), forward)
        };
        self.0
            .pipeline_factory
            .preflight(mode, forward.as_deref())?;

        let previous_pipeline = self.0.state.lock().await.pipeline.take();

        if let Some(pipeline) = previous_pipeline {
            if let Err(error) = pipeline.stop().await {
                self.restore_pipeline(previous_mode, previous_forward).await;
                return Err(error);
            }
        }

        let replacement = self.0.receiver.as_ref().map(|receiver| {
            PipelineHandle::start(
                &self.0.config,
                mode,
                forward.clone(),
                receiver.clone(),
                self.0.store.clone(),
                self.0.capture.clone(),
            )
        });
        let mut state = self.0.state.lock().await;
        state.pipeline = replacement;
        state.mode = mode;
        state.forward = forward.clone();
        drop(state);
        self.0.store.clear().await;
        Ok((mode, forward))
    }

    async fn restore_pipeline(&self, mode: Mode, forward: Option<String>) {
        let pipeline = self.0.receiver.as_ref().map(|receiver| {
            PipelineHandle::start(
                &self.0.config,
                mode,
                forward.clone(),
                receiver.clone(),
                self.0.store.clone(),
                self.0.capture.clone(),
            )
        });
        let mut state = self.0.state.lock().await;
        state.pipeline = pipeline;
        state.mode = mode;
        state.forward = forward;
    }
}
