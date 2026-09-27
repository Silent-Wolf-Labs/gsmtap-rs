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
  renderPacketTable(node, [packet], 'listen', jest.fn());
  expect(node.querySelector('table')).not.toBeNull();
  expect(node.querySelectorAll('tbody tr')).toHaveLength(1);
  expect(node.querySelectorAll('thead th')).toHaveLength(tableColumns('listen').length);
  expect(node.querySelector('.packet-details-row')).toBeNull();
  expect(node.querySelector('summary')).toBeNull();

  node.querySelector('tbody tr td').click();
  expect(node.querySelectorAll(':scope > table > tbody > tr')).toHaveLength(2);
  expect(node.querySelector('.packet-details-row .gsmtap-header-table').textContent).toContain('42');
});

test('collapses details when the expanded row is clicked again', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  const packet = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 } };
  renderPacketTable(node, [packet], 'listen', jest.fn(), { onSelectPacket });
  const row = node.querySelector('tbody tr');

  row.click();
  row.click();

  expect(row.classList.contains('selected-row')).toBe(false);
  expect(node.querySelector('.packet-details-row')).toBeNull();
  expect(onSelectPacket).toHaveBeenNthCalledWith(1, packet);
  expect(onSelectPacket).toHaveBeenNthCalledWith(2, null);
});

test('moves details to the newly selected row', () => {
  const node = document.createElement('div');
  const packets = [
    { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'first', rawHex: 'CA', decoded: { arfcn: 42 }, parseError: null, modified: false },
    { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'second', rawHex: 'FE', decoded: { arfcn: 43 }, parseError: null, modified: false },
  ];
  renderPacketTable(node, packets, 'listen', jest.fn());
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
  renderPacketTable(node, [packet], 'listen', jest.fn(), { onSelectPacket });

  node.querySelector('tbody tr td').click();
  renderPacketTable(node, [packet], 'listen', jest.fn(), { selectedPacketId: 1, onSelectPacket });

  expect(node.querySelector('.selected-row').textContent).toContain('#1');
  expect(node.querySelector('.packet-details-row .gsmtap-header-table').textContent).toContain('42');
});

test('preserves Original Input disclosure state for the same selected packet', () => {
  const node = document.createElement('div');
  const packet = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 } };
  renderPacketTable(node, [packet], 'listen', jest.fn());
  node.querySelector('tbody > tr').click();
  node.querySelector('.packet-original-input').open = true;

  renderPacketTable(node, [packet], 'listen', jest.fn(), { selectedPacketId: 1 });

  expect(node.querySelector('.packet-original-input').open).toBe(true);
});

test('closes Original Input when a different packet is selected', () => {
  const node = document.createElement('div');
  const first = { id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'first', rawHex: 'CA', decoded: { arfcn: 42 } };
  const second = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'second', rawHex: 'FE', decoded: { arfcn: 43 } };
  renderPacketTable(node, [first, second], 'listen', jest.fn());
  const rows = node.querySelectorAll(':scope > table > tbody > tr');
  rows[0].click();
  node.querySelector('.packet-original-input').open = true;
  rows[1].click();

  expect(node.querySelector('.packet-original-input').open).toBe(false);
});

test('clears unavailable selected packet state', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  renderPacketTable(node, [], 'listen', jest.fn(), { selectedPacketId: 1, onSelectPacket });
  expect(onSelectPacket).toHaveBeenCalledWith(null);
});

test('clears a selected packet that is absent from a nonempty table', () => {
  const node = document.createElement('div');
  const onSelectPacket = jest.fn();
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null };

  renderPacketTable(node, [packet], 'listen', jest.fn(), { selectedPacketId: 1, onSelectPacket });
  expect(onSelectPacket).toHaveBeenCalledWith(null);
});

test('clicking a modify-mode row expands details without loading it for editing', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 7, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 }, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'modify', onModify);
  node.querySelector('tbody tr td').click();
  expect(node.querySelector('.packet-details-row')).not.toBeNull();
  expect(onModify).not.toHaveBeenCalled();
});

test('provides only the Modify action for editable packets', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 10, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'modify', onModify);
  let modify = [...node.querySelectorAll('button')].find(button => button.textContent === 'Modify');
  expect(modify).toBeUndefined();
  node.querySelector('tbody tr td').click();
  expect(node.querySelectorAll('button')).toHaveLength(1);
  expect(node.querySelector('button').textContent).toBe('Modify');
  modify = node.querySelector('button');
  modify.click();
  expect(node.querySelectorAll('.packet-details-row')).toHaveLength(1);
  expect(onModify).toHaveBeenCalledWith(packet);
});

test('supports keyboard row selection while ignoring unrelated keys', () => {
  const node = document.createElement('div');
  const packet = { id: 8, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn());
  const row = node.querySelector('tbody tr');
  row.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', bubbles: true }));
  expect(row.getAttribute('aria-selected')).toBe('false');
  row.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
  expect(row.getAttribute('aria-selected')).toBe('true');
  row.dispatchEvent(new KeyboardEvent('keydown', { key: ' ', bubbles: true }));
  expect(row.getAttribute('aria-selected')).toBe('false');
  expect(node.querySelector('.packet-details-row')).toBeNull();
});

test('keeps the row expanded when the details Modify action is clicked', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 8, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null };

  renderPacketTable(node, [packet], 'modify', onModify);
  node.querySelector('tbody tr td').click();
  node.querySelector('button').click();
  expect(node.querySelector('tbody tr').classList.contains('selected-row')).toBe(true);
  expect(onModify).toHaveBeenCalledWith(packet);
});

test('renders not sent relay status when no forwarding result exists', () => {
  const node = document.createElement('div');
  const packet = { id: 9, direction: 'TX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {} };

  renderPacketTable(node, [packet], 'relay', jest.fn());
  expect(node.querySelector('tbody tr').textContent).toContain('Not sent');
});

test('renders empty state and replaces selected details', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [], 'listen', jest.fn());
  expect(node.textContent).toContain('No packets captured');
  renderPacketTable(node, [], 'listen', jest.fn(), { hasPackets: true });
  expect(node.textContent).toContain('No packets match');
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, parseError: null, modified: false };
  renderPacketTable(node, [packet], 'listen', jest.fn());
  node.querySelector('tbody tr td').click();
  renderPacketTable(node, [packet], 'listen', jest.fn());
  expect(node.querySelector('.packet-details-row')).toBeNull();
});

test('renders errors, forwarding state, modified bytes, and modify actions', () => {
  const node = document.createElement('div');
  const onModify = jest.fn();
  const packet = { id: 3, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'DE AD', decoded: { arfcn: 1 }, parseError: null, forwardStatus: 'sent', modified: true, originalRawHex: 'CA FE', finalRawHex: 'DE AD' };
  renderPacketTable(node, [packet], 'modify', onModify);
  expect(node.textContent).not.toContain('Original: CA FE');
  node.querySelector('tbody tr td').click();
  expect(node.textContent).toContain('sent');
  expect(node.querySelector('.packet-original-input pre').textContent).toBe('CA FE');
  [...node.querySelectorAll('button')].find(button => button.textContent === 'Modify').click();
  expect(onModify).toHaveBeenCalledWith(packet);

  const errorNode = document.createElement('div');
  renderPacketTable(errorNode, [{ ...packet, parseError: 'invalid header', decoded: null }], 'listen', onModify);
  errorNode.querySelector('tbody tr td').click();
  expect(errorNode.querySelector('.packet-details-row .error').textContent).toBe('invalid header');
});

test('renders explicit and compatibility source addresses', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [
    { id: 1, direction: 'RX', timestampMs: Date.now(), sourceAddress: '192.0.2.1:4729', peer: 'legacy', decoded: {} },
    { id: 2, direction: 'TX', timestampMs: Date.now(), decoded: {} },
  ], 'listen', jest.fn());

  expect(tableColumns('listen')).toContain('Source address');
  const rows = [...node.querySelectorAll('tbody tr')].filter(row => !row.classList.contains('packet-details-row'));
  expect(rows[0].cells[3].textContent).toBe('—');
  expect(rows[1].cells[3].textContent).toBe('192.0.2.1:4729');
});

test('renders the per-packet relay result in its own column', () => {
  const node = document.createElement('div');
  const packet = { id: 4, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA FE', decoded: {}, parseError: null, forwardStatus: 'sent', modified: false };
  renderPacketTable(node, [packet], 'relay', jest.fn());
  expect(node.querySelectorAll('th')[6].textContent).toBe('Forward');
  expect(node.querySelector('tbody tr').textContent).toContain('sent');
});

test('renders mode-appropriate header filters and emits categorical changes', () => {
  const node = document.createElement('div');
  const onFilterChange = jest.fn();
  renderPacketTable(node, [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }], 'listen', jest.fn(), {
    filters: { decode: null, sourceAddress: '' },
    onFilterChange,
  });

  expect([...node.querySelectorAll('.packet-column-filter-trigger')].map(button => button.getAttribute('aria-label')))
    .toEqual(['Filter Packet', 'Filter Timestamp', 'Filter Source address', 'Filter Decode']);
  node.querySelector('[aria-label="Filter Decode"]').click();
  const failed = node.querySelector('input[value="error"]');
  failed.checked = true;
  failed.dispatchEvent(new Event('change', { bubbles: true }));
  expect(onFilterChange).toHaveBeenCalledWith({ field: 'decode', value: 'error' });
});

test('renders Modify-only filters and marks active filters', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [{ id: 1, direction: 'TX', timestampMs: Date.now(), decoded: {}, modified: true }], 'modify', jest.fn(), {
    filters: { direction: 'TX', decode: null, modified: 'yes', sourceAddress: '' },
    onFilterChange: jest.fn(),
  });

  expect(node.querySelectorAll('.packet-column-filter-trigger')).toHaveLength(6);
  expect(node.querySelector('[aria-label="Filter Direction, active"]')).not.toBeNull();
  expect(node.querySelector('[aria-label="Filter Modified, active"]')).not.toBeNull();
  expect(node.querySelector('[aria-label="Filter Decode"]')).not.toBeNull();
});

test('supports keyboard closing and text entry in a header filter', () => {
  const node = document.createElement('div');
  const onFilterChange = jest.fn();
  renderPacketTable(node, [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }], 'listen', jest.fn(), {
    filters: { decode: null, sourceAddress: '' },
    onFilterChange,
  });
  const trigger = node.querySelector('[aria-label="Filter Source address"]');
  trigger.click();
  const search = trigger.closest('.packet-column-filter').querySelector('.packet-column-search');
  search.value = '192.168';
  search.dispatchEvent(new Event('input', { bubbles: true }));
  expect(onFilterChange).toHaveBeenCalledWith({ field: 'sourceAddress', value: '192.168' });
  search.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  expect(trigger.getAttribute('aria-expanded')).toBe('false');
});

test('opens only the selected column filter', () => {
  const node = document.createElement('div');
  renderPacketTable(node, [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }], 'listen', jest.fn(), {
    filters: { packet: '', timestamp: '', decode: null, sourceAddress: '' },
    onFilterChange: jest.fn(),
  });

  const packetTrigger = node.querySelector('[aria-label="Filter Packet"]');
  const sourceTrigger = node.querySelector('[aria-label="Filter Source address"]');
  sourceTrigger.click();

  expect(sourceTrigger.getAttribute('aria-expanded')).toBe('true');
  expect(sourceTrigger.closest('.packet-column-filter').querySelector('.packet-column-filter-menu').classList.contains('open')).toBe(true);
  expect(packetTrigger.getAttribute('aria-expanded')).toBe('false');
  expect(packetTrigger.closest('.packet-column-filter').querySelector('.packet-column-filter-menu').classList.contains('open')).toBe(false);
});

test('renders selection column and checkboxes exclusively in relay mode', () => {
  const node = document.createElement('div');
  const packets = [
    { id: 10, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {} },
    { id: 11, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CB', decoded: {} },
  ];

  // In listen mode: no select column or checkboxes
  renderPacketTable(node, packets, 'listen', jest.fn());
  expect(node.querySelector('.packet-column-select')).toBeNull();
  expect(node.querySelector('.packet-row-checkbox')).toBeNull();

  // In relay mode: select column and checkboxes present
  const onSelectionChange = jest.fn();
  const onSelectAllChange = jest.fn();
  const selectedPacketIds = new Set([10]);

  renderPacketTable(node, packets, 'relay', jest.fn(), {
    selectedPacketIds,
    onSelectionChange,
    onSelectAllChange,
  });

  const headerSelect = node.querySelector('.packet-select-all');
  expect(headerSelect).not.toBeNull();
  expect(headerSelect.getAttribute('aria-label')).toBe('Select all visible packets');
  expect(headerSelect.indeterminate).toBe(true);
  expect(headerSelect.checked).toBe(false);

  const rowCheckboxes = node.querySelectorAll('.packet-row-checkbox');
  expect(rowCheckboxes).toHaveLength(2);
  // Note: reverse ordering in table: first row is #11, second is #10
  const checkbox11 = node.querySelector('input[aria-label="Select packet 11"]');
  const checkbox10 = node.querySelector('input[aria-label="Select packet 10"]');
  expect(checkbox11.checked).toBe(false);
  expect(checkbox10.checked).toBe(true);

  // Changing row checkbox triggers onSelectionChange and does not expand row
  checkbox11.checked = true;
  checkbox11.dispatchEvent(new Event('change', { bubbles: true }));
  expect(onSelectionChange).toHaveBeenCalledWith(11, true);
  expect(node.querySelector('.packet-details-row')).toBeNull();

  // Clicking row checkbox directly does not expand details
  checkbox11.click();
  expect(node.querySelector('.packet-details-row')).toBeNull();

  // Header select-all change event
  headerSelect.checked = true;
  headerSelect.dispatchEvent(new Event('change', { bubbles: true }));
  expect(onSelectAllChange).toHaveBeenCalledWith([10, 11], true);
});

test('header select-all reflects fully selected and unselected states', () => {
  const node = document.createElement('div');
  const packets = [
    { id: 10, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {} },
    { id: 11, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CB', decoded: {} },
  ];

  // All selected
  renderPacketTable(node, packets, 'relay', jest.fn(), {
    selectedPacketIds: new Set([10, 11]),
  });
  const selectAll = node.querySelector('.packet-select-all');
  expect(selectAll.checked).toBe(true);
  expect(selectAll.indeterminate).toBe(false);

  // None selected
  renderPacketTable(node, packets, 'relay', jest.fn(), {
    selectedPacketIds: new Set(),
  });
  expect(node.querySelector('.packet-select-all').checked).toBe(false);
  expect(node.querySelector('.packet-select-all').indeterminate).toBe(false);
});

test('expands details with correct colSpan in relay mode', () => {
  const node = document.createElement('div');
  const packet = { id: 10, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {} };
  renderPacketTable(node, [packet], 'relay', jest.fn());
  node.querySelector('tbody tr td:nth-child(2)').click();
  const detailsCell = node.querySelector('.packet-details-row td');
  expect(detailsCell).not.toBeNull();
  expect(detailsCell.colSpan).toBe(7);
});
