export function createNavigationController(documentRef = document) {
  const selector = documentRef.querySelector('#workbench-view');
  const panels = Array.from(documentRef.querySelectorAll('[data-workbench-view]'));
  let started = false;
  function select(view) {
    if (!panels.some(panel => panel.dataset.workbenchView === view)) return;
    for (const panel of panels) panel.hidden = panel.dataset.workbenchView !== view;
    if (selector) selector.value = view;
  }
  const onChange = () => select(selector.value);
  // Hide views the server was built without, falling back to packets.
  function restrict(views) {
    const allowed = new Set(['packets', ...views]);
    for (const option of selector?.options ?? []) {
      option.hidden = !allowed.has(option.value);
      option.disabled = option.hidden;
    }
    const current = selector?.value ?? panels.find(panel => !panel.hidden)?.dataset.workbenchView;
    if (!allowed.has(current)) select('packets');
  }
  return {
    select,
    restrict,
    start() {
      if (started || !selector) return;
      started = true;
      selector.addEventListener('change', onChange);
      select(selector.value);
    },
    stop() { selector?.removeEventListener('change', onChange); started = false; },
  };
}
