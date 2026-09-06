import { normalizePacket, packetDecodeState, packetModifiedState, packetSourceAddress } from '../../static/models/packet-model.js';

test('normalizes source addresses while preserving packet data', () => {
  const packet = { id: 1, peer: '192.0.2.1:4729', decoded: {} };
  const normalized = normalizePacket(packet);
  expect(normalized.sourceAddress).toBe('192.0.2.1:4729');
  expect(packetSourceAddress(packet)).toBe('192.0.2.1:4729');
  expect(normalized.id).toBe(1);
});

test('prefers the explicit source address over the compatibility peer field', () => {
  expect(packetSourceAddress({ sourceAddress: '198.51.100.2:4729', peer: 'legacy' })).toBe('198.51.100.2:4729');
  expect(packetSourceAddress({})).toBeNull();
});

test('derives decode and modification states', () => {
  expect(packetDecodeState({ decoded: {} })).toBe('success');
  expect(packetDecodeState({ parseError: 'invalid' })).toBe('error');
  expect(packetDecodeState({})).toBe('unknown');
  expect(packetModifiedState({ modified: true })).toBe(true);
  expect(packetModifiedState({ modified: false })).toBe(false);
});
