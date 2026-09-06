import { packetSourceAddress, relayForwardingState } from '../../models/packet-model.js';
import { renderPacketDetails } from './packet-schema.js';
import { applyTooltip } from '../tooltip.js';

function addText(parent, tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

export function tableColumns(activeMode) {
  const columns = ['Packet', 'Direction', 'Timestamp', 'Source address', 'Decode'];
  if (activeMode === 'relay') columns.push('Forward');
  if (activeMode === 'modify') columns.push('Modified');
  return columns;
}

const columnTooltips = {
  Packet: 'Unique packet-history record number.',
  Direction: 'Traffic direction: RX was received; TX was sent by the workbench.',
  Timestamp: 'Time when the workbench captured or sent the packet.',
  'Source address': 'UDP endpoint that sent the packet to the workbench or local endpoint used to transmit it.',
  Decode: 'Whether the packet decoded successfully as GSMTAP.',
  Forward: 'Result of forwarding this relay packet to the configured UDP target.',
  Modified: 'Whether the packet was explicitly edited before sending.',
};

export function renderPacketTable(node, packets, activeMode, onModify, {
  selectedPacketId = null,
  onSelectPacket,
} = {}) {
  const previousOriginalInput = node.querySelector('.selected-row + .packet-details-row .packet-original-input');
  const previousOriginalInputOpen = previousOriginalInput?.open ?? false;
  node.replaceChildren();
  if (!packets.length) {
    if (selectedPacketId !== null) onSelectPacket?.(null);
    addText(node, 'p', 'No packets match the current filters.', 'empty-state');
    return;
  }
  const selectedPacket = packets.find(packet => packet.id === selectedPacketId);
  if (selectedPacketId !== null && !selectedPacket) onSelectPacket?.(null);
  let expandedPacketId = selectedPacket?.id ?? null;
  const table = document.createElement('table');
  const head = table.createTHead().insertRow();
  const showModified = activeMode === 'modify';
  const showForward = activeMode === 'relay';
  for (const header of tableColumns(activeMode)) {
    const heading = addText(head, 'th', header);
    applyTooltip(heading, header, columnTooltips[header]);
  }
  const body = table.createTBody();
  const clearExpansion = () => {
    for (const selectedRow of body.querySelectorAll('.selected-row')) {
      selectedRow.classList.remove('selected-row');
      selectedRow.setAttribute('aria-selected', 'false');
    }
    for (const detailsRow of body.querySelectorAll('.packet-details-row')) detailsRow.remove();
  };

  const expandPacket = (row, packet) => {
    if (expandedPacketId === packet.id && row.classList.contains('selected-row')
      && body.querySelector('.packet-details-row')) return;

    clearExpansion();

    row.classList.add('selected-row');
    row.setAttribute('aria-selected', 'true');

    const detailsRow = body.insertRow();
    detailsRow.className = 'packet-details-row';
    body.insertBefore(detailsRow, row.nextSibling);
    const detailsCell = detailsRow.insertCell();
    detailsCell.colSpan = tableColumns(activeMode).length;
    detailsCell.append(renderPacketDetails(packet, {
      onModify,
      showModifyActions: activeMode === 'modify',
      originalInputOpen: packet.id === selectedPacketId ? previousOriginalInputOpen : false,
    }));
    expandedPacketId = packet.id;
    onSelectPacket?.(packet);
  };

  const collapsePacket = () => {
    clearExpansion();
    expandedPacketId = null;
    onSelectPacket?.(null);
  };

  for (const packet of packets.slice().reverse()) {
    const row = body.insertRow();
    const packetCell = addText(row, 'td', `#${packet.id}`);
    addText(row, 'td', packet.direction);
    addText(row, 'td', new Date(Number(packet.timestampMs)).toLocaleString());
    addText(row, 'td', packetSourceAddress(packet) || '—');
    addText(row, 'td', packet.parseError ? 'Error' : 'Success', packet.parseError ? 'error' : 'success');
    if (showForward) {
      const forwarding = relayForwardingState(packet);
      addText(row, 'td', forwarding.status, forwarding.failed ? 'error' : packet.forwardStatus ? 'success' : '');
    }
    if (showModified) addText(row, 'td', packet.modified ? 'Yes' : 'No');

    row.tabIndex = 0;
    row.setAttribute('aria-selected', 'false');
    if (packet === selectedPacket) expandPacket(row, packet);
    row.addEventListener('click', event => {
      if (event.target.closest('button')) return;
      if (expandedPacketId === packet.id) collapsePacket();
      else expandPacket(row, packet);
    });
    row.addEventListener('keydown', event => {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      event.preventDefault();
      row.click();
    });
  }
  node.append(table);
}
