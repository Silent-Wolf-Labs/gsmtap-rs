use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};

#[derive(Clone, Default)]
pub struct CaptureControl {
    paused: Arc<AtomicBool>,
}

impl CaptureControl {
    pub fn is_paused(&self) -> bool {
        self.paused.load(Ordering::Acquire)
    }
    pub fn set_paused(&self, paused: bool) {
        self.paused.store(paused, Ordering::Release);
    }
}
