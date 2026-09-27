export function subscribeToUpdates(onUpdate, { EventSourceImpl = EventSource, delay = 250 } = {}) {
  const events = new EventSourceImpl('/api/events');
  let refreshScheduled = false;
  let timeout;

  events.onmessage = () => {
    if (refreshScheduled) return;
    refreshScheduled = true;
    timeout = setTimeout(() => {
      refreshScheduled = false;
      timeout = undefined;
      onUpdate();
    }, delay);
  };

  return () => {
    if (timeout) clearTimeout(timeout);
    events.close();
  };
}
