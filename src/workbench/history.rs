use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, Ordering};
use tokio::sync::{broadcast, Mutex};

use super::dto::PacketRecord;

pub struct PacketStore {
    packets: Mutex<VecDeque<PacketRecord>>,
    capacity: usize,
    events: broadcast::Sender<PacketRecord>,
    next_id: AtomicU64,
    counters: RuntimeCounters,
}

#[derive(Default)]
pub struct RuntimeCounters {
    received: AtomicU64,
    ingress_dropped: AtomicU64,
    history_dropped: AtomicU64,
    parse_failed: AtomicU64,
    forward_sent: AtomicU64,
    forward_failed: AtomicU64,
    ui_events_dropped: AtomicU64,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RuntimeStats {
    pub received: u64,
    pub ingress_dropped: u64,
    pub history_dropped: u64,
    pub parse_failed: u64,
    pub forward_sent: u64,
    pub forward_failed: u64,
    pub ui_events_dropped: u64,
}

impl RuntimeCounters {
    pub fn received(&self) {
        self.received.fetch_add(1, Ordering::Relaxed);
    }
    pub fn ingress_dropped(&self) {
        self.ingress_dropped.fetch_add(1, Ordering::Relaxed);
    }
    pub fn history_dropped(&self) {
        self.history_dropped.fetch_add(1, Ordering::Relaxed);
    }
    pub fn parse_failed(&self) {
        self.parse_failed.fetch_add(1, Ordering::Relaxed);
    }
    pub fn forward_sent(&self) {
        self.forward_sent.fetch_add(1, Ordering::Relaxed);
    }
    pub fn forward_failed(&self) {
        self.forward_failed.fetch_add(1, Ordering::Relaxed);
    }
    pub fn ui_events_dropped(&self, count: u64) {
        self.ui_events_dropped.fetch_add(count, Ordering::Relaxed);
    }
    pub fn snapshot(&self) -> RuntimeStats {
        RuntimeStats {
            received: self.received.load(Ordering::Relaxed),
            ingress_dropped: self.ingress_dropped.load(Ordering::Relaxed),
            history_dropped: self.history_dropped.load(Ordering::Relaxed),
            parse_failed: self.parse_failed.load(Ordering::Relaxed),
            forward_sent: self.forward_sent.load(Ordering::Relaxed),
            forward_failed: self.forward_failed.load(Ordering::Relaxed),
            ui_events_dropped: self.ui_events_dropped.load(Ordering::Relaxed),
        }
    }
}

impl PacketStore {
    pub fn new(capacity: usize) -> Self {
        let (events, _) = broadcast::channel(capacity.max(1));
        Self {
            packets: Mutex::new(VecDeque::with_capacity(capacity)),
            capacity: capacity.max(1),
            events,
            next_id: AtomicU64::new(1),
            counters: RuntimeCounters::default(),
        }
    }

    pub async fn record(&self, mut packet: PacketRecord) {
        packet.id = self.next_id.fetch_add(1, Ordering::Relaxed);
        let mut packets = self.packets.lock().await;
        if packets.len() >= self.capacity {
            packets.pop_front();
            self.counters.history_dropped();
        }
        packets.push_back(packet.clone());
        drop(packets);
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

    #[cfg(test)]
    pub async fn list(&self) -> Vec<PacketRecord> {
        self.packets.lock().await.iter().cloned().collect()
    }

    pub async fn list_recent(&self, limit: usize) -> Vec<PacketRecord> {
        let packets = self.packets.lock().await;
        let start = packets.len().saturating_sub(limit);
        packets.iter().skip(start).cloned().collect()
    }

    pub fn subscribe(&self) -> broadcast::Receiver<PacketRecord> {
        self.events.subscribe()
    }

    pub fn counters(&self) -> &RuntimeCounters {
        &self.counters
    }
}
