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

export const editableHeaderFields = [
  ['Version', 'version'], ['Header Length', 'headerLengthWords'], ['Type', 'messageType'],
  ['Timeslot', 'timeslot'], ['ARFCN', 'arfcn'], ['Signal', 'signalDbm'], ['SNR', 'snrDb'],
  ['Frame', 'frameNumber'], ['Subtype', 'subtype'], ['Antenna', 'antennaNumber'],
  ['Sub-slot', 'subSlot'], ['Reserved', 'reserved'],
];

export const editableFieldLabels = { extensionHex: 'Header extension', payloadHex: 'Payload' };

export function normalizePacket(packet) {
  return { ...packet, sourceAddress: packet.sourceAddress ?? packet.peer ?? null };
}

export function packetSourceAddress(packet) {
  return normalizePacket(packet).sourceAddress;
}

export function packetDecodeState(packet) {
  return packet.parseError ? 'error' : packet.decoded ? 'success' : 'unknown';
}

export function packetModifiedState(packet) {
  return Boolean(packet.modified);
}

export function relayForwardingState(packet) {
  const status = packet.forwardStatus || 'Not sent';
  return { status, failed: status.startsWith('error:') };
}
