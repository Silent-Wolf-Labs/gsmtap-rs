import { jest } from '@jest/globals';
import { fieldGroups, fieldTooltips, hexFieldNames, packetFieldNames, renderDecodedFields, renderPacketDetails } from '../../static/components/packet/packet-schema.js';

test('exposes shared GSMTAP field metadata', () => {
  expect(packetFieldNames).toHaveLength(12);
  expect(hexFieldNames).toEqual(['extensionHex', 'payloadHex']);
  expect(fieldGroups.flatMap(([, names]) => names)).toEqual(packetFieldNames);
  expect([...packetFieldNames, ...hexFieldNames].every(name => fieldTooltips[name])).toBe(true);
});

test('renders structured header, payload, and forwarding status', () => {
  const node = renderPacketDetails({ rawHex: 'CA FE', decoded: {
    version: 2, headerLengthWords: 4, messageType: 1, timeslot: 3, arfcn: 42,
    signalDbm: -71, snrDb: 18, frameNumber: 103482, subtype: 0, antennaNumber: 0,
    payloadHex: '03 03 01\n06 1B',
  }, forwardStatus: 'sent' });
  expect(node.querySelector('.packet-details-header h3').textContent).toBe('GSMTAP Header');
  expect(node.querySelectorAll('.gsmtap-header-table thead th')).toHaveLength(10);
  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('42');
  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('4 words');
  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('-71 dBm');
  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('18 dB');
  expect(node.querySelector('.packet-payload pre').textContent).toBe('03 03 01\n06 1B');
  expect(node.textContent).toContain('sent');
  expect(node.querySelector('.packet-payload')).not.toBeNull();
});

test('renders decoded fields with reusable tooltip metadata', () => {
  const fields = renderDecodedFields({ arfcn: 42, customField: 'value' });
  const arfcn = fields.querySelector('dt');
  expect(arfcn.textContent).toBe('arfcn');
  expect(arfcn.title).toBe(fieldTooltips.arfcn);
  expect(arfcn.getAttribute('aria-label')).toContain(fieldTooltips.arfcn);
  expect(fields.querySelectorAll('dt')).toHaveLength(2);
});

test('renders null decoded values and fallback field tooltips', () => {
  const fields = renderDecodedFields({ customField: null });

  expect(fields.querySelector('dt').getAttribute('aria-label')).toContain('Decoded GSMTAP field: customField.');
  expect(fields.querySelector('dd').textContent).toBe('');
  expect(renderDecodedFields(null).children).toHaveLength(0);
});

test('renders parse errors without pretending decoded data exists', () => {
  const node = renderPacketDetails({ rawHex: 'CA', decoded: null, parseError: 'invalid header' });
  expect(node.querySelector('.error').textContent).toBe('invalid header');
  expect(node.querySelector('.packet-details-header')).toBeNull();
  expect(node.querySelector('.packet-original-input pre').textContent).toBe('CA');
});

test('uses supplied header length values and preserves original input disclosure', () => {
  const node = renderPacketDetails({
    rawHex: 'DE AD',
    originalRawHex: 'CA FE',
    decoded: { headerLengthWords: 4, payloadHex: ' CA  FE ' },
  });

  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('4 words');
  expect(node.querySelector('.gsmtap-header-table').textContent).not.toContain('16 bytes');
  const disclosure = node.querySelector('.packet-original-input');
  expect(disclosure.open).toBe(false);
  expect(disclosure.querySelector('summary').textContent).toBe('Original Input');
  expect(disclosure.querySelector('pre').textContent).toBe('CA FE');
  disclosure.open = true;
  expect(disclosure.querySelector('pre').textContent).toBe('CA FE');
  expect(node.querySelector('.packet-payload pre').textContent).toBe(' CA  FE ');
});

test('restores the Original Input disclosure when requested', () => {
  const node = renderPacketDetails({ rawHex: 'CA' }, { originalInputOpen: true });

  expect(node.querySelector('.packet-original-input').open).toBe(true);
});

test('displays supplied byte and word header lengths without deriving either value', () => {
  const node = renderPacketDetails({ decoded: { headerLengthBytes: 16, headerLengthWords: 4, payloadHex: '' } });

  expect(node.querySelector('.gsmtap-header-table').textContent).toContain('16 bytes (4 words)');
});

test('uses neutral placeholders for missing header and payload values', () => {
  const node = renderPacketDetails({ rawHex: 'CA', decoded: {} });

  expect([...node.querySelectorAll('.gsmtap-header-table tbody td')].every(cell => cell.textContent === '—')).toBe(true);
  expect(node.querySelector('.packet-payload pre').textContent).toBe('—');
});

test('keeps modify actions opt-in', () => {
  const packet = { id: 7, direction: 'RX', decoded: { arfcn: 1 }, rawHex: 'CA' };
  const onReplay = jest.fn();
  const onModify = jest.fn();
  const passive = renderPacketDetails(packet, { onReplay, onModify });
  expect(passive.querySelectorAll('button')).toHaveLength(0);
  const editable = renderPacketDetails(packet, { onReplay, onModify, showModifyActions: true });
  expect(editable.querySelectorAll('button')).toHaveLength(2);
});

test('renders each optional modify action independently', () => {
  const packet = { id: 8, direction: 'RX', decoded: { arfcn: 1 }, rawHex: 'CA' };
  expect(renderPacketDetails(packet, { showModifyActions: true, onReplay: jest.fn() }).querySelectorAll('button')).toHaveLength(1);
  expect(renderPacketDetails(packet, { showModifyActions: true, onModify: jest.fn() }).querySelectorAll('button')).toHaveLength(1);
  expect(renderPacketDetails({ ...packet, direction: 'TX' }, { showModifyActions: true, onReplay: jest.fn(), onModify: jest.fn() }).querySelectorAll('button')).toHaveLength(0);
});
