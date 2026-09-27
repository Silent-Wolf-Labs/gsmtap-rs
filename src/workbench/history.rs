use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};
use tokio::sync::{broadcast, Mutex};

use super::dto::PacketRecord;

const DEFAULT_MAX_HISTORY_BYTES: usize = 256 * 1024 * 1024;
const MAX_EVENT_CAPACITY: usize = 1_024;

pub struct PacketStore {
    packets: Mutex<VecDeque<PacketRecord>>,
    capacity: usize,
    bytes: AtomicUsize,
    max_bytes: usize,
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
    capture_skipped: AtomicU64,
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
    pub capture_skipped: u64,
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
    pub fn capture_skipped(&self) {
        self.capture_skipped.fetch_add(1, Ordering::Relaxed);
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
            capture_skipped: self.capture_skipped.load(Ordering::Relaxed),
        }
    }
}

impl PacketStore {
    pub fn new(capacity: usize) -> Self {
        let (events, _) = broadcast::channel(capacity.clamp(1, MAX_EVENT_CAPACITY));
        Self {
            packets: Mutex::new(VecDeque::with_capacity(capacity.min(MAX_EVENT_CAPACITY))),
            capacity: capacity.max(1),
            bytes: AtomicUsize::new(0),
            max_bytes: DEFAULT_MAX_HISTORY_BYTES,
            events,
            next_id: AtomicU64::new(1),
            counters: RuntimeCounters::default(),
        }
    }

    pub async fn record(&self, mut packet: PacketRecord) {
        packet.id = self.next_id.fetch_add(1, Ordering::Relaxed);
        let packet_bytes = packet_memory_bytes(&packet);
        let mut packets = self.packets.lock().await;
        while packets.len() >= self.capacity
            || (!packets.is_empty()
                && self
                    .bytes
                    .load(Ordering::Relaxed)
                    .saturating_add(packet_bytes)
                    > self.max_bytes)
        {
            if let Some(evicted) = packets.pop_front() {
                self.bytes
                    .fetch_sub(packet_memory_bytes(&evicted), Ordering::Relaxed);
            }
            self.counters.history_dropped();
        }
        self.bytes.fetch_add(packet_bytes, Ordering::Relaxed);
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

    pub async fn update_forward_status(&self, id: u64, status: Option<String>) -> bool {
        let mut packets = self.packets.lock().await;
        if let Some(packet) = packets.iter_mut().find(|packet| packet.id == id) {
            packet.forward_status = status;
            let packet = packet.clone();
            drop(packets);
            let _ = self.events.send(packet);
            true
        } else {
            false
        }
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

    pub async fn clear(&self) {
        self.packets.lock().await.clear();
        self.bytes.store(0, Ordering::Relaxed);
    }

    pub fn subscribe(&self) -> broadcast::Receiver<PacketRecord> {
        self.events.subscribe()
    }

    pub fn counters(&self) -> &RuntimeCounters {
        &self.counters
    }
}

fn packet_memory_bytes(packet: &PacketRecord) -> usize {
    let decoded_bytes = packet.decoded.as_ref().map_or(0, |decoded| {
        decoded.extension_hex.len() + decoded.payload_hex.len()
    });
    packet.raw_hex.len()
        + packet.peer.len()
        + packet.source_address.as_ref().map_or(0, String::len)
        + packet.destination.as_ref().map_or(0, String::len)
        + packet.parse_error.as_ref().map_or(0, String::len)
        + packet.forward_status.as_ref().map_or(0, String::len)
        + packet.original_raw_hex.as_ref().map_or(0, String::len)
        + packet.final_raw_hex.as_ref().map_or(0, String::len)
        + decoded_bytes
}
