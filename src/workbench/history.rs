use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, Ordering};
use tokio::sync::{broadcast, Mutex};

use super::dto::PacketRecord;

pub struct PacketStore {
    packets: Mutex<VecDeque<PacketRecord>>,
    capacity: usize,
    events: broadcast::Sender<PacketRecord>,
    next_id: AtomicU64,
}

impl PacketStore {
    pub fn new(capacity: usize) -> Self {
        let (events, _) = broadcast::channel(capacity.max(1));
        Self {
            packets: Mutex::new(VecDeque::with_capacity(capacity)),
            capacity: capacity.max(1),
            events,
            next_id: AtomicU64::new(1),
        }
    }

    pub async fn record(&self, mut packet: PacketRecord) {
        packet.id = self.next_id.fetch_add(1, Ordering::Relaxed);
        let mut packets = self.packets.lock().await;
        if packets.len() >= self.capacity {
            packets.pop_front();
        }
        packets.push_back(packet.clone());
        let _ = self.events.send(packet);
    }

    pub async fn get(&self, id: u64) -> Option<PacketRecord> {
        self.packets
            .lock()
            .await
            .iter()
            .find(|packet| packet.id == id)
            .cloned()
    }

    pub async fn list(&self) -> Vec<PacketRecord> {
        self.packets.lock().await.iter().cloned().collect()
    }

    pub fn subscribe(&self) -> broadcast::Receiver<PacketRecord> {
        self.events.subscribe()
    }
}
