import { jest } from '@jest/globals';
import { renderPacketTable, tableColumns } from '../../static/components/packet-table.js';

test('uses mode-specific table columns', () => {
  expect(tableColumns('listen')).not.toContain('Modified');
  expect(tableColumns('relay')).not.toContain('Modified');
  expect(tableColumns('modify')).toContain('Modified');
  expect(tableColumns('listen')).toContain('Decode');
  expect(tableColumns('relay')).toContain('Forward');
  expect(tableColumns('listen')).not.toContain('Forward');
});

test('renders packet rows with expandable details', () => {
  const node = document.createElement('div');
  const packet = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: '127.0.0.1:4729', rawHex: 'CA FE', decoded: { arfcn: 42 }, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  expect(node.querySelector('table')).not.toBeNull();
  expect(node.querySelectorAll('tbody tr')).toHaveLength(1);
  expect(node.querySelector('summary').textContent).toBe('Expand');
  expect(node.querySelector('pre').textContent).toContain('42');
});

test('renders empty state and preserves an opened row', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [], 'listen', jest.fn(), jest.fn());
  expect(node.textContent).toContain('No packets match');
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  node.querySelector('details').open = true;
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  expect(node.querySelector('details').open).toBe(true);
});

test('renders errors, forwarding state, modified bytes, and modify actions', () => {
  const node = document.createElement('div');
  const onReplay = jest.fn();
  const onModify = jest.fn();
  const packet = { id: 3, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'DE AD', decoded: { arfcn: 1 }, parseError: null, forwardStatus: 'sent', modified: true, originalRawHex: 'CA FE', finalRawHex: 'DE AD' };
  renderPacketTable(node, [packet], 'modify', onReplay, onModify);
  expect(node.textContent).toContain('sent');
  expect(node.textContent).toContain('Original: CA FE');
  node.querySelector('details').open = true;
  node.querySelectorAll('button')[0].click();
  node.querySelectorAll('button')[1].click();
  expect(onReplay).toHaveBeenCalledWith(3);
  expect(onModify).toHaveBeenCalledWith(packet);

  const errorNode = document.createElement('div');
  renderPacketTable(errorNode, [{ ...packet, parseError: 'invalid header', decoded: null }], 'listen', onReplay, onModify);
  expect(errorNode.querySelector('details > div .error').textContent).toBe('invalid header');
});

test('renders the per-packet relay result in its own column', () => {
  const node = document.createElement('div');
  const packet = { id: 4, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA FE', decoded: {}, parseError: null, forwardStatus: 'sent', modified: false };
  renderPacketTable(node, [packet], 'relay', jest.fn(), jest.fn());
  expect(node.querySelectorAll('th')[5].textContent).toBe('Forward');
  expect(node.querySelector('tbody tr').textContent).toContain('sent');
});
