import { jest } from '@jest/globals';
import { renderStatusCard, statusFields } from '../../static/components/cards/status-card.js';

const status = mode => ({
  capturePaused: false,
  mode,
  gsmtapListen: '127.0.0.1:4729',
  gsmtapForward: 'target:4729',
  stats: {
    received: 8,
    parseFailed: 1,
    forwardSent: 8,
    forwardFailed: 0,
    ingressDropped: 0,
    historyDropped: 0,
    uiEventsDropped: 0,
    captureSkipped: 0,
  },
});

test('status fields contain the compact primary information', () => {
  expect(statusFields(status('listen')).map(([label]) => label)).toEqual([
    'Workbench Mode', 'Listen on', 'Forward to',
  ]);
  expect(statusFields(status('listen')).find(([label]) => label === 'Forward to')[1]).toBe('target:4729');
});

test('renders mode, labeled endpoints, and no diagnostic counters', () => {
  const node = document.createElement('div');
  renderStatusCard(node, status('listen'));

  expect(node.querySelector('.mode-control').textContent).toBe('Listen▾');
  expect(node.textContent).toContain('Workbench Mode:');
  expect(node.textContent).toContain('Listen on:');
  expect(node.textContent).toContain('127.0.0.1:4729');
  expect(node.textContent).toContain('Forward to:');
  expect(node.textContent).toContain('target:4729');
  expect(node.textContent).not.toContain('received');
  expect(node.textContent).not.toContain('decode failure');
  expect(node.textContent).not.toContain('queue drops');
  expect(node.querySelector('.status-layout')).not.toBeNull();
});

test('renders listen mode without a forwarding destination as disabled', () => {
  const node = document.createElement('div');
  renderStatusCard(node, { ...status('listen'), gsmtapForward: null });

  expect(node.textContent).toContain('Forward to:Disabled');
});

test('renders the pause icon and label while capture is active', () => {
  const node = document.createElement('div');
  renderStatusCard(node, status('listen'));
  const button = node.querySelector('.capture-toggle');

  expect(button.textContent).toBe('Pause Capture');
  expect(button.querySelector('img').src).toContain('/styles/pause-icon-32x32.png');
  expect(button.querySelector('img').getAttribute('alt')).toBe('');
  expect(button.getAttribute('aria-pressed')).toBe('false');
  expect(button.classList.contains('capture-paused')).toBe(false);
});

test('renders the play icon and label while capture is paused', () => {
  const node = document.createElement('div');
  renderStatusCard(node, { ...status('listen'), capturePaused: true });
  const button = node.querySelector('.capture-toggle');

  expect(button.textContent).toBe('Resume Capture');
  expect(button.querySelector('img').src).toContain('/styles/play-icon-32x32.png');
  expect(button.getAttribute('aria-pressed')).toBe('true');
  expect(button.classList.contains('capture-paused')).toBe(true);
});

test('disables the toggle while updating and reports failures', async () => {
  const node = document.createElement('div');
  let resolve;
  const update = jest.fn(() => new Promise(done => { resolve = done; }));
  renderStatusCard(node, status('listen'), { onCaptureToggle: update });
  const button = node.querySelector('.capture-toggle');
  const click = button.click();
  expect(button.disabled).toBe(true);
  resolve();
  await click;
  expect(update).toHaveBeenCalledWith(true);

  renderStatusCard(node, status('listen'), { onCaptureToggle: async () => { throw new Error('offline'); } });
  const failed = node.querySelector('.capture-toggle');
  failed.click();
  await Promise.resolve();
  await Promise.resolve();
  expect(failed.disabled).toBe(false);
  expect(node.querySelector('.capture-error').textContent).toContain('offline');
});
