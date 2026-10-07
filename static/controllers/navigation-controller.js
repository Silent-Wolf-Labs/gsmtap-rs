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
  return {
    select,
    start() {
      if (started || !selector) return;
      started = true;
      selector.addEventListener('change', onChange);
      select(selector.value);
    },
    stop() { selector?.removeEventListener('change', onChange); started = false; },
  };
}
