use std::collections::VecDeque;
use tokio::sync::{broadcast, Mutex};

use super::dto::PacketRecord;

pub struct PacketStore {
    packets: Mutex<VecDeque<PacketRecord>>,
    capacity: usize,
    events: broadcast::Sender<PacketRecord>,
}

impl PacketStore {
    pub fn new(capacity: usize) -> Self {
        let (events, _) = broadcast::channel(capacity.max(1));
        Self {
            packets: Mutex::new(VecDeque::with_capacity(capacity)),
            capacity: capacity.max(1),
            events,
        }
    }

    pub async fn record(&self, packet: PacketRecord) {
        let mut packets = self.packets.lock().await;
        if packets.len() >= self.capacity {
            packets.pop_front();
        }
        packets.push_back(packet.clone());
        let _ = self.events.send(packet);
    }

    pub async fn list(&self) -> Vec<PacketRecord> {
        self.packets.lock().await.iter().cloned().collect()
    }

    pub fn subscribe(&self) -> broadcast::Receiver<PacketRecord> {
        self.events.subscribe()
    }
}
