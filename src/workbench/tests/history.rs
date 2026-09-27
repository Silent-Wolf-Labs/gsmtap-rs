use super::super::{config::Mode, history::PacketStore, network::receive_loop};
use super::socket;
use std::sync::Arc;

#[tokio::test]
async fn history_evicts_oldest_packets_and_counts_evictions() {
    let receiver = socket().await;
    let receiver_addr = receiver.local_addr().unwrap();
    let source = socket().await;
    let store = Arc::new(PacketStore::new(1));
    let task = tokio::spawn(receive_loop(
        receiver,
        store.clone(),
        Mode::Listen,
        None,
        Arc::new(socket().await),
    ));
    for payload in [0xca, 0xcb] {
        source
            .send_to(
                &[2, 4, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, payload],
                receiver_addr,
            )
            .await
            .unwrap();
    }
    tokio::time::timeout(std::time::Duration::from_secs(1), async {
        while store.counters().snapshot().received < 2
            || store.counters().snapshot().history_dropped < 1
        {
            tokio::task::yield_now().await;
        }
    })
    .await
    .unwrap();
    let history = store.list().await;
    assert_eq!(history.len(), 1);
    assert!(history[0].raw_hex.ends_with("CB"));
    assert_eq!(store.counters().snapshot().history_dropped, 1);
    task.abort();
}
