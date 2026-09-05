import { jest } from '@jest/globals';
import { fieldGroups, fieldTooltips, hexFieldNames, packetFieldNames, renderDecodedFields, renderPacketDetails } from '../../static/components/packet/packet-schema.js';

test('exposes shared GSMTAP field metadata', () => {
  expect(packetFieldNames).toHaveLength(12);
  expect(hexFieldNames).toEqual(['extensionHex', 'payloadHex']);
  expect(fieldGroups.flatMap(([, names]) => names)).toEqual(packetFieldNames);
  expect([...packetFieldNames, ...hexFieldNames].every(name => fieldTooltips[name])).toBe(true);
});

test('renders raw bytes, decoded fields, and forwarding status', () => {
  const node = renderPacketDetails({ rawHex: 'CA FE', decoded: { arfcn: 42 }, forwardStatus: 'sent' });
  expect(node.textContent).toContain('CA FE');
  expect(node.textContent).toContain('sent');
  expect(node.querySelector('pre').textContent).toContain('42');
});

test('renders decoded fields with reusable tooltip metadata', () => {
  const fields = renderDecodedFields({ arfcn: 42, customField: 'value' });
  const arfcn = fields.querySelector('dt');
  expect(arfcn.textContent).toBe('arfcn');
  expect(arfcn.title).toBe(fieldTooltips.arfcn);
  expect(arfcn.getAttribute('aria-label')).toContain(fieldTooltips.arfcn);
  expect(fields.querySelectorAll('dt')).toHaveLength(2);
});

test('renders parse errors without pretending decoded data exists', () => {
  const node = renderPacketDetails({ rawHex: 'CA', decoded: null, parseError: 'invalid header' });
  expect(node.querySelector('.error').textContent).toBe('invalid header');
  expect(node.querySelector('pre')).toBeNull();
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
