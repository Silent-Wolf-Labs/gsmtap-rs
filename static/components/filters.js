import { packetSourceAddress } from '../models/packet-model.js';

export const defaultPacketFilters = Object.freeze({
  packet: '',
  direction: null,
  timestamp: '',
  decode: null,
  modified: null,
  sourceAddress: '',
});

const definitions = [
  { id: 'packet', label: 'Packet' },
  { id: 'direction', label: 'Direction', options: [['RX', 'RX'], ['TX', 'TX']] },
  { id: 'timestamp', label: 'Timestamp' },
  { id: 'decode', label: 'Decode', options: [['success', 'Decoded'], ['error', 'Failed']] },
  { id: 'modified', label: 'Modified', options: [['yes', 'Yes'], ['no', 'No']] },
  { id: 'sourceAddress', label: 'Source address' },
];

export function visibleFilterIds(mode) {
  return definitions
    .filter(definition =>
      !(definition.id === 'direction' && mode !== 'modify') &&
      !(definition.id === 'modified' && mode !== 'modify'))
    .map(definition => definition.id);
}

export function filterPackets(packets, filters = defaultPacketFilters) {
  const direction = filters.direction;
  const packetQuery = String(filters.packet ?? '').trim().toLocaleLowerCase();
  const timestamp = String(filters.timestamp ?? '').trim().toLocaleLowerCase();
  const decode = filters.decode;
  const modified = filters.modified;
  const sourceAddress = String(filters.sourceAddress ?? '').trim().toLocaleLowerCase();
  return packets.filter(packet =>
    (!packetQuery || String(packet.id ?? '').toLocaleLowerCase().includes(packetQuery)) &&
    (!timestamp || new Date(Number(packet.timestampMs)).toLocaleString().toLocaleLowerCase().includes(timestamp)) &&
    (!direction || packet.direction === direction) &&
    (!decode || (decode === 'success') === !packet.parseError) &&
    (!modified || (modified === 'yes') === Boolean(packet.modified)) &&
    (!sourceAddress || String(packetSourceAddress(packet) ?? '').toLocaleLowerCase().includes(sourceAddress)));
}

export function isFilterActive(filters = defaultPacketFilters, mode = 'listen') {
  return visibleFilterIds(mode).some(field => {
    const value = filters[field];
    return typeof value === 'string' ? Boolean(value.trim()) : value !== null && value !== undefined;
  });
}

export function filterDefinition(field) {
  return definitions.find(definition => definition.id === field);
}
