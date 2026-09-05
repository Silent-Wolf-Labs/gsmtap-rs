import { jest } from '@jest/globals';
import { renderPacketTable, tableColumns } from '../../static/components/packet/packet-table.js';

test('uses mode-specific table columns', () => {
  expect(tableColumns('listen')).not.toContain('Modified');
  expect(tableColumns('relay')).not.toContain('Modified');
  expect(tableColumns('modify')).toContain('Modified');
  expect(tableColumns('listen')).toContain('Decode');
  expect(tableColumns('relay')).toContain('Forward');
  expect(tableColumns('listen')).not.toContain('Forward');
  expect(tableColumns('listen')).not.toContain('Details');
  expect(tableColumns('relay')).not.toContain('Details');
  expect(tableColumns('modify')).not.toContain('Details');
});

test('shows packet details only after its row is selected', () => {
  const node = document.createElement('div');
  const packet = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: '127.0.0.1:4729', rawHex: 'CA FE', decoded: { arfcn: 42 }, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  expect(node.querySelector('table')).not.toBeNull();
  expect(node.querySelectorAll('tbody tr')).toHaveLength(1);
  expect(node.querySelectorAll('thead th')).toHaveLength(tableColumns('listen').length);
  expect(node.querySelector('.packet-details-row')).toBeNull();
  expect(node.querySelector('summary')).toBeNull();

  node.querySelector('tbody tr td').click();
  expect(node.querySelectorAll('tbody tr')).toHaveLength(2);
  expect(node.querySelector('.packet-details-row pre').textContent).toContain('42');
});

test('moves details to the newly selected row', () => {
  const node = document.createElement('div');
  const packets = [
    { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'first', rawHex: 'CA', decoded: { arfcn: 42 }, parseError: null, modified: false },
    { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'second', rawHex: 'FE', decoded: { arfcn: 43 }, parseError: null, modified: false },
  ];
  renderPacketTable(node, packets, 'listen', jest.fn(), jest.fn());
  const rows = node.querySelectorAll('tbody tr');
  rows[0].querySelector('td').click();
  expect(rows[0].classList.contains('selected-row')).toBe(true);
  expect(node.querySelector('.packet-details-row').textContent).toContain('43');

  rows[1].querySelector('td').click();
  expect(rows[0].classList.contains('selected-row')).toBe(false);
  expect(rows[1].classList.contains('selected-row')).toBe(true);
  expect(node.querySelectorAll('.packet-details-row')).toHaveLength(1);
  expect(node.querySelector('.packet-details-row').textContent).toContain('42');
});

test('restores selected packet details after a table refresh', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  const packet = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 }, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn(), { onSelectPacket });

  node.querySelector('tbody tr td').click();
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn(), { selectedPacketId: 1, onSelectPacket });

  expect(node.querySelector('.selected-row').textContent).toContain('#1');
  expect(node.querySelector('.packet-details-row pre').textContent).toContain('42');
});

test('clears unavailable selected packet state', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  renderPacketTable(node, [], 'listen', jest.fn(), jest.fn(), { selectedPacketId: 1, onSelectPacket });
  expect(onSelectPacket).toHaveBeenCalledWith(null);
});

test('clears a selected packet that is absent from a nonempty table', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null };

  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn(), { selectedPacketId: 1, onSelectPacket });
  expect(onSelectPacket).toHaveBeenCalledWith(null);
});

test('selecting a modify-mode row loads it for editing', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 7, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 }, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'modify', jest.fn(), onModify);
  node.querySelector('tbody tr td').click();
  expect(onModify).toHaveBeenCalledWith(packet);
});

test('provides an always-visible select action for modifyable packets', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 10, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'modify', jest.fn(), onModify);
  const select = [...node.querySelectorAll('button')].find(button => button.textContent === 'Select');
  expect(select).not.toBeUndefined();
  select.click();
  expect(onModify).toHaveBeenCalledWith(packet);
});

test('supports keyboard row selection while ignoring unrelated keys', () => {
  const node = document.createElement('div');
  const packet = { id: 8, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  const row = node.querySelector('tbody tr');
  row.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', bubbles: true }));
  expect(row.getAttribute('aria-selected')).toBe('false');
  row.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  expect(row.getAttribute('aria-selected')).toBe('true');
});

test('does not select a row when a nested button is clicked', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 8, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null };

  renderPacketTable(node, [packet], 'modify', jest.fn(), onModify);
  node.querySelector('button').click();
  expect(node.querySelector('tbody tr').classList.contains('selected-row')).toBe(true);
  expect(onModify).toHaveBeenCalledWith(packet);
});

test('renders pending relay status when no forwarding result exists', () => {
  const node = document.createElement('div');
  const packet = { id: 9, direction: 'TX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {} };

  renderPacketTable(node, [packet], 'relay', jest.fn(), jest.fn());
  expect(node.querySelector('tbody tr').textContent).toContain('Pending');
});

test('renders empty state and replaces selected details', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [], 'listen', jest.fn(), jest.fn());
  expect(node.textContent).toContain('No packets match');
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  node.querySelector('tbody tr td').click();
  renderPacketTable(node, [packet], 'listen', jest.fn(), jest.fn());
  expect(node.querySelector('.packet-details-row')).toBeNull();
});

test('renders errors, forwarding state, modified bytes, and modify actions', () => {
  const node = document.createElement('div');
  const onReplay = jest.fn();
  const onModify = jest.fn();
  const packet = { id: 3, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'DE AD', decoded: { arfcn: 1 }, parseError: null, forwardStatus: 'sent', modified: true, originalRawHex: 'CA FE', finalRawHex: 'DE AD' };
  renderPacketTable(node, [packet], 'modify', onReplay, onModify);
  expect(node.textContent).not.toContain('Original: CA FE');
  node.querySelector('tbody tr td').click();
  expect(node.textContent).toContain('sent');
  expect(node.textContent).toContain('Original: CA FE');
  [...node.querySelectorAll('button')].find(button => button.textContent === 'Replay').click();
  [...node.querySelectorAll('button')].find(button => button.textContent === 'Modify').click();
  expect(onReplay).toHaveBeenCalledWith(3);
  expect(onModify).toHaveBeenCalledWith(packet);

  const errorNode = document.createElement('div');
  renderPacketTable(errorNode, [{ ...packet, parseError: 'invalid header', decoded: null }], 'listen', onReplay, onModify);
  errorNode.querySelector('tbody tr td').click();
  expect(errorNode.querySelector('.packet-details-row .error').textContent).toBe('invalid header');
});

test('renders the per-packet relay result in its own column', () => {
  const node = document.createElement('div');
  const packet = { id: 4, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA FE', decoded: {}, parseError: null, forwardStatus: 'sent', modified: false };
  renderPacketTable(node, [packet], 'relay', jest.fn(), jest.fn());
  expect(node.querySelectorAll('th')[5].textContent).toBe('Forward');
  expect(node.querySelector('tbody tr').textContent).toContain('sent');
});
