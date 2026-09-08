import { jest } from '@jest/globals';
import { defaultPacketFilters, filterPackets, isFilterActive, visibleFilterIds } from '../../static/components/filters.js';

const packets = [
  { direction: 'RX', parseError: undefined, modified: false, sourceAddress: '192.168.1.20:4729' },
  { direction: 'RX', parseError: 'truncated', modified: false, sourceAddress: null },
  { direction: 'TX', parseError: undefined, modified: true },
];

test('exposes only applicable controls by mode', () => {
  expect(visibleFilterIds('listen')).toEqual(['packet', 'timestamp', 'decode', 'sourceAddress']);
  expect(visibleFilterIds('relay')).toEqual(['packet', 'timestamp', 'decode', 'sourceAddress']);
  expect(visibleFilterIds('modify')).toEqual(['packet', 'direction', 'timestamp', 'decode', 'modified', 'sourceAddress']);
});

test('filters successful received packets', () => {
  expect(filterPackets(packets, { direction: 'RX', decode: 'success' })).toHaveLength(1);
});

test('uses all as the default value for omitted filters', () => {
  expect(filterPackets(packets, {})).toEqual(packets);
});

test('filters parse failures and modified packets', () => {
  expect(filterPackets(packets, { decode: 'error' })).toHaveLength(1);
  expect(filterPackets(packets, { direction: 'TX', decode: 'success', modified: 'yes' })).toHaveLength(1);
});

test('matches source address text case-insensitively and handles missing values', () => {
  expect(filterPackets(packets, { sourceAddress: '192.168.1.' })).toHaveLength(1);
  expect(filterPackets(packets, { sourceAddress: ':4729' })).toHaveLength(1);
  expect(filterPackets(packets, { sourceAddress: 'missing' })).toHaveLength(0);
});

test('matches packet ids and displayed timestamps by partial text', () => {
  const timestamp = Date.parse('2025-01-02T03:04:05.000Z');
  const timestampText = new Date(timestamp).toLocaleString().slice(0, 4);
  expect(filterPackets([{ id: 42, timestampMs: timestamp }], { packet: '2' })).toHaveLength(1);
  expect(filterPackets([{ id: 42, timestampMs: timestamp }], { timestamp: timestampText })).toHaveLength(1);
  expect(filterPackets([{ id: 42, timestampMs: timestamp }], { packet: '99' })).toHaveLength(0);
});

test('identifies active filters using the current mode', () => {
  expect(isFilterActive(defaultPacketFilters, 'listen')).toBe(false);
  expect(isFilterActive({ ...defaultPacketFilters, direction: 'TX' }, 'listen')).toBe(false);
  expect(isFilterActive({ ...defaultPacketFilters, direction: 'TX' }, 'modify')).toBe(true);
  expect(isFilterActive({ ...defaultPacketFilters, sourceAddress: '192.0.2.' }, 'listen')).toBe(true);
});

test('applies all active filters together', () => {
  expect(filterPackets(packets, {
    direction: 'RX', decode: 'success', modified: null, sourceAddress: '192.168',
  })).toEqual([packets[0]]);
});
