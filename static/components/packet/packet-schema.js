import { applyTooltip } from '../tooltip.js';
import { editableFieldLabels, editableHeaderFields, fieldGroups, fieldTooltips, hexFieldNames, packetFieldNames, relayForwardingState } from '../../models/packet-model.js';

export { editableFieldLabels, editableHeaderFields, fieldGroups, fieldTooltips, hexFieldNames, packetFieldNames, relayForwardingState };

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

const detailHeaderFields = [
  ['Version', 'version'],
  ['Header Length', 'headerLengthWords'],
  ['Type', 'messageType'],
  ['Timeslot', 'timeslot'],
  ['ARFCN', 'arfcn'],
  ['Signal', 'signalDbm'],
  ['SNR', 'snrDb'],
  ['Frame', 'frameNumber'],
  ['Subtype', 'subtype'],
  ['Antenna', 'antennaNumber'],
];

function displayValue(value, suffix = '') {
  return value == null ? '—' : `${value}${suffix}`;
}

export function displayHeaderLength(decoded) {
  const words = decoded?.headerLengthWords;
  const bytes = decoded?.headerLengthBytes;
  if (bytes != null && words != null) return `${bytes} bytes (${words} words)`;
  if (bytes != null) return `${bytes} bytes`;
  if (words != null) return `${words} words`;
  return '—';
}

export function displayHeaderValue(decoded, name) {
  if (name === 'headerLengthWords') return displayHeaderLength(decoded);
  if (name === 'signalDbm') return displayValue(decoded?.[name], ' dBm');
  if (name === 'snrDb') return displayValue(decoded?.[name], ' dB');
  return displayValue(decoded?.[name]);
}

function renderHeaderTable(decoded, documentRef) {
  const scroll = documentRef.createElement('div');
  scroll.className = 'gsmtap-header-scroll';
  const table = documentRef.createElement('table');
  table.className = 'gsmtap-header-table';
  const head = table.createTHead().insertRow();
  const values = table.createTBody().insertRow();

  for (const [label, name] of detailHeaderFields) {
    const heading = documentRef.createElement('th');
    heading.scope = 'col';
    heading.textContent = label;
    head.append(heading);
    const value = documentRef.createElement('td');
    value.textContent = displayHeaderValue(decoded, name);
    values.append(value);
  }

  scroll.append(table);
  return scroll;
}

export function renderPacketDetails(packet, {
  documentRef = document,
  onModify,
  showModifyActions = false,
  originalInputOpen = false,
} = {}) {
  const content = documentRef.createElement('div');
  content.className = 'packet-details';
  if (packet.forwardStatus) addText(content, documentRef, 'em', ` ${packet.forwardStatus}`, 'packet-forward-status');

  if (packet.decoded) {
    const header = documentRef.createElement('section');
    header.className = 'packet-details-header';
    addText(header, documentRef, 'h3', 'GSMTAP Header');
    header.append(renderHeaderTable(packet.decoded, documentRef));
    content.append(header);

    const payload = documentRef.createElement('section');
    payload.className = 'packet-payload';
    addText(payload, documentRef, 'h3', 'Payload');
    const payloadText = documentRef.createElement('pre');
    payloadText.textContent = packet.decoded.payloadHex ?? '—';
    payload.append(payloadText);
    content.append(payload);
  } else if (packet.parseError) {
    addText(content, documentRef, 'p', packet.parseError, 'error');
  }

  const original = documentRef.createElement('details');
  original.className = 'packet-original-input';
  original.open = originalInputOpen;
  addText(original, documentRef, 'summary', 'Original Input');
  const originalText = documentRef.createElement('pre');
  originalText.textContent = packet.originalRawHex ?? packet.rawHex ?? '—';
  original.append(originalText);
  content.append(original);

  if (showModifyActions && packet.direction === 'RX' && packet.decoded) {
    const actions = documentRef.createElement('div');
    actions.className = 'packet-detail-actions';
    if (onModify) {
      const modify = documentRef.createElement('button');
      modify.type = 'button';
      modify.textContent = 'Modify';
      modify.addEventListener('click', () => onModify(packet));
      actions.append(modify);
    }
    content.append(actions);
  }
  return content;
}
