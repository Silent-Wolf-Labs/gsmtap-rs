import { applyTooltip } from '../tooltip.js';

export const packetFieldNames = ['version', 'headerLengthWords', 'messageType', 'timeslot', 'arfcn', 'signalDbm', 'snrDb', 'frameNumber', 'subtype', 'antennaNumber', 'subSlot', 'reserved'];
export const hexFieldNames = ['extensionHex', 'payloadHex'];

export const fieldGroups = [
  ['Header', ['version', 'headerLengthWords', 'messageType']],
  ['Radio metadata', ['timeslot', 'arfcn', 'signalDbm', 'snrDb', 'frameNumber']],
  ['Message metadata', ['subtype', 'antennaNumber', 'subSlot', 'reserved']],
];

export const fieldTooltips = {
  version: 'GSMTAP header version.', headerLengthWords: 'Length of the GSMTAP header, measured in 32-bit words.',
  messageType: 'Protocol or radio message type carried by this packet.', timeslot: 'TDMA timeslot used by the radio message.',
  arfcn: 'Absolute Radio Frequency Channel Number (ARFCN).', signalDbm: 'Received signal strength in dBm.',
  snrDb: 'Signal-to-noise ratio in dB.', frameNumber: 'Radio frame number associated with the message.',
  subtype: 'Subtype of the GSMTAP message.', antennaNumber: 'Antenna or receiver chain number.',
  subSlot: 'Sub-slot used by the radio message, when applicable.', reserved: 'Reserved header field; normally left at zero.',
  extensionHex: 'Optional GSMTAP header extension bytes, written as hexadecimal.', payloadHex: 'Message payload bytes, written as hexadecimal.'
};

export function relayForwardingState(packet) {
  const status = packet.forwardStatus || 'Pending';
  return { status, failed: status.startsWith('error:') };
}

function addText(parent, documentRef, tag, text, className) {
  const node = documentRef.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

export function renderDecodedFields(decoded, { documentRef = document } = {}) {
  const fields = documentRef.createElement('dl');
  fields.className = 'decoded-fields';
  for (const [name, value] of Object.entries(decoded || {})) {
    const label = documentRef.createElement('dt');
    label.textContent = name;
    applyTooltip(label, name, fieldTooltips[name] || `Decoded GSMTAP field: ${name}.`);
    const detail = documentRef.createElement('dd');
    detail.textContent = value == null ? '' : String(value);
    fields.append(label, detail);
  }
  return fields;
}

export function renderPacketDetails(packet, {
  documentRef = document,
  onReplay,
  onModify,
  showModifyActions = false,
} = {}) {
  const content = documentRef.createElement('div');
  addText(content, documentRef, 'code', packet.rawHex);
  if (packet.forwardStatus) addText(content, documentRef, 'em', ` ${packet.forwardStatus}`);
  if (packet.parseError) addText(content, documentRef, 'p', packet.parseError, 'error');
  else addText(content, documentRef, 'pre', JSON.stringify(packet.decoded, null, 2));
  if (packet.modified) addText(content, documentRef, 'pre', `Original: ${packet.originalRawHex}\nFinal: ${packet.finalRawHex}`);
  if (showModifyActions && packet.direction === 'RX' && packet.decoded) {
    if (onReplay) {
      const replay = documentRef.createElement('button');
      replay.type = 'button';
      replay.textContent = 'Replay';
      replay.addEventListener('click', () => onReplay(packet.id));
      content.append(replay);
    }
    if (onModify) {
      const modify = documentRef.createElement('button');
      modify.type = 'button';
      modify.textContent = 'Modify';
      modify.addEventListener('click', () => onModify(packet));
      content.append(modify);
    }
  }
  return content;
}
