import { jest } from '@jest/globals';
import { renderStatusCard, statusFields } from '../../static/components/cards/status-card.js';

const status = (mode, forward = mode === 'listen' ? null : '127.0.0.1:4729') => ({
  capturePaused: false, mode, gsmtapListen: '127.0.0.1:4729', gsmtapForward: forward,
  stats: {},
});

test('hides the forward address completely in listen mode', () => {
  const node = document.createElement('div');
  renderStatusCard(node, status('listen'));
  expect(statusFields(status('listen')).map(([label]) => label)).toEqual(['Workbench Mode', 'Listen on']);
  expect(node.querySelector('.status-row:nth-child(3)').hidden).toBe(true);
  expect(node.textContent).not.toContain('Forward to:');
});

test('saves a listener address without changing the current mode', async () => {
  const node = document.createElement('div');
  const onListenChange = jest.fn(async () => {});
  renderStatusCard(node, status('listen'), { onListenChange });
  const input = node.querySelector('.listen-address');
  expect(input.value).toBe('127.0.0.1:4729');
  input.value = '10.200.0.10:4729';
  node.querySelector('.listen-address-save').click();
  await Promise.resolve();
  expect(onListenChange).toHaveBeenCalledWith('10.200.0.10:4729');
});

test('requires an address before activating relay from listen mode', async () => {
  const node = document.createElement('div');
  const onModeChange = jest.fn(async () => {});
  renderStatusCard(node, status('listen'), { onModeChange });
  node.querySelector('.mode-control').click();
  node.querySelector('[data-mode="relay"]').click();
  expect(node.querySelector('.status-row:nth-child(3)').hidden).toBe(false);
  expect(node.querySelector('.forward-address-save').textContent).toBe('Activate Relay');
  node.querySelector('.forward-address-save').click();
  expect(node.querySelector('.forward-address-error').textContent).toContain('Enter a forward address');
  node.querySelector('.forward-address').value = '127.0.0.1:14729';
  node.querySelector('.forward-address-save').click();
  await Promise.resolve();
  expect(onModeChange).toHaveBeenCalledWith('relay', '127.0.0.1:14729');
});

test('retains address while changing relay to modify and clears it after listen confirms', async () => {
  const node = document.createElement('div');
  const onModeChange = jest.fn(async () => {});
  renderStatusCard(node, status('relay'), { onModeChange });
  const input = node.querySelector('.forward-address');
  expect(input.value).toBe('127.0.0.1:4729');
  node.querySelector('.mode-control').click();
  node.querySelector('[data-mode="modify"]').click();
  await Promise.resolve();
  expect(onModeChange).toHaveBeenCalledWith('modify');
  renderStatusCard(node, status('modify'), { onModeChange });
  expect(input.value).toBe('127.0.0.1:4729');
  renderStatusCard(node, status('listen'), { onModeChange });
  expect(node.querySelector('.status-row:nth-child(3)').hidden).toBe(true);
  expect(input.value).toBe('');
});

test('saves an address change in an active forwarding mode', async () => {
  const node = document.createElement('div');
  const onModeChange = jest.fn(async () => {});
  renderStatusCard(node, status('modify'), { onModeChange });
  node.querySelector('.forward-address').value = 'relay.example:4729';
  node.querySelector('.forward-address-save').click();
  await Promise.resolve();
  expect(onModeChange).toHaveBeenCalledWith('modify', 'relay.example:4729');
});

test('keeps the card and capture control in place across status refreshes', () => {
  const node = document.createElement('div'); renderStatusCard(node, status('listen'));
  const card = node.querySelector('.status-card'); const button = node.querySelector('.capture-toggle');
  renderStatusCard(node, { ...status('listen'), capturePaused: true });
  expect(node.querySelector('.status-card')).toBe(card); expect(node.querySelector('.capture-toggle')).toBe(button);
  expect(button.textContent).toBe('Resume Capture');
});

test('keeps the mode menu open during an ordinary status refresh', () => {
  const node = document.createElement('div'); renderStatusCard(node, status('listen'));
  const control = node.querySelector('.mode-control'); const menu = node.querySelector('.mode-menu');
  control.click();
  renderStatusCard(node, { ...status('listen'), capturePaused: true });
  expect(menu.hidden).toBe(false);
});
