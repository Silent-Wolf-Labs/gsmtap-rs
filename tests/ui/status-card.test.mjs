import { renderStatusCard, statusFields } from '../../static/components/cards/status-card.js';

const status = mode => ({ mode, gsmtapListen: '127.0.0.1:4729', gsmtapForward: 'target:4729', stats: { received: 8, parseFailed: 1, forwardSent: 8, forwardFailed: 0, ingressDropped: 0, historyDropped: 0, uiEventsDropped: 0 } });

test('omits forwarding from listen status', () => {
  expect(statusFields(status('listen')).some(([label]) => label === 'Forward target')).toBe(false);
  expect(statusFields(status('relay')).some(([label]) => label === 'Forward target')).toBe(true);
});

test('renders mode, endpoint, metrics, and diagnostic counters', () => {
  const node = document.createElement('div');
  renderStatusCard(node, status('listen'));
  expect(node.querySelector('.mode-badge').textContent).toBe('LISTEN');
  expect(node.textContent).toContain('127.0.0.1:4729');
  expect(node.textContent).toContain('8 received');
  expect(node.textContent).toContain('1 decode failure');
  expect(node.textContent).toContain('queue drops');
  expect(node.textContent).not.toContain('Forward target');
});

test('renders relay target and warning styling for nonzero diagnostics', () => {
  const node = document.createElement('div');
  renderStatusCard(node, { ...status('relay'), stats: { received: 8, parseFailed: 1, forwardSent: 7, forwardFailed: 1, ingressDropped: 2, historyDropped: 3, uiEventsDropped: 4 } });
  expect(node.textContent).toContain('→ target:4729');
  expect(node.textContent).toContain('7 forwarded');
  expect(node.textContent).toContain('1 forward failures');
  expect(node.querySelectorAll('.warning')).toHaveLength(5);
});

test('renders modify mode with a disabled forward target and no relay warnings', () => {
  const node = document.createElement('div');
  renderStatusCard(node, { ...status('modify'), gsmtapForward: '', stats: { received: 8, parseFailed: 0, forwardSent: 0, forwardFailed: 0, ingressDropped: 0, historyDropped: 0, uiEventsDropped: 0 } });

  expect(node.textContent).toContain('→ Disabled');
  expect(node.textContent).not.toContain('forwarded');
  expect(node.querySelectorAll('.warning')).toHaveLength(0);
  expect(statusFields({ ...status('modify'), gsmtapForward: '' }).find(([label]) => label === 'Forward target')[1]).toBe('Disabled');
});
