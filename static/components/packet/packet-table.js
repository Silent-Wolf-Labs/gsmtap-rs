import { relayForwardingState, renderPacketDetails } from './packet-schema.js';
import { applyTooltip } from '../tooltip.js';

function addText(parent, tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

export function tableColumns(activeMode) {
  const columns = ['Packet', 'Direction', 'Timestamp', 'Peer', 'Decode'];
  if (activeMode === 'relay') columns.push('Forward');
  if (activeMode === 'modify') columns.push('Modified');
  columns.push('Details');
  return columns;
}

const columnTooltips = {
  Packet: 'Unique packet-history record number.',
  Direction: 'Traffic direction: RX was received; TX was sent by the workbench.',
  Timestamp: 'Time when the workbench captured or sent the packet.',
  Peer: 'Remote UDP endpoint associated with the packet.',
  Decode: 'Whether the packet decoded successfully as GSMTAP.',
  Forward: 'Result of forwarding this relay packet to the configured UDP target.',
  Modified: 'Whether the packet was explicitly edited before sending.',
  Details: 'Expand this row to view raw bytes, decoded fields, errors, and actions.',
};

export function renderPacketTable(node, packets, activeMode, onReplay, onModify) {
  const openPackets = new Set([...node.querySelectorAll('details[data-packet-id][open]')].map(details => details.dataset.packetId));
  node.replaceChildren();
  if (!packets.length) {
    addText(node, 'p', 'No packets match the current filters.', 'empty-state');
    return;
  }
  const table = document.createElement('table');
  const head = table.createTHead().insertRow();
  const showModified = activeMode === 'modify';
  const showForward = activeMode === 'relay';
  for (const header of tableColumns(activeMode)) {
    const heading = addText(head, 'th', header);
    applyTooltip(heading, header, columnTooltips[header]);
  }
  const body = table.createTBody();
  for (const packet of packets.slice().reverse()) {
    const row = body.insertRow();
    addText(row, 'td', `#${packet.id}`);
    addText(row, 'td', packet.direction);
    addText(row, 'td', new Date(Number(packet.timestampMs)).toLocaleString());
    addText(row, 'td', packet.peer);
    addText(row, 'td', packet.parseError ? 'Error' : 'Success', packet.parseError ? 'error' : 'success');
    if (showForward) {
      const forwarding = relayForwardingState(packet);
      addText(row, 'td', forwarding.status, forwarding.failed ? 'error' : packet.forwardStatus ? 'success' : '');
    }
    if (showModified) addText(row, 'td', packet.modified ? 'Yes' : 'No');
    const details = document.createElement('details');
    details.dataset.packetId = packet.id;
    details.open = openPackets.has(String(packet.id));
    const summary = document.createElement('summary');
    summary.textContent = 'Expand';
    details.append(summary, renderPacketDetails(packet, {
      onReplay,
      onModify,
      showModifyActions: activeMode === 'modify',
    }));
    const detailsCell = row.insertCell();
    detailsCell.append(details);
    if (activeMode === 'modify' && packet.direction === 'RX' && packet.decoded) {
      const select = document.createElement('button');
      select.type = 'button';
      select.textContent = 'Select';
      applyTooltip(select, 'Select', 'Select this packet for modification.');
      select.addEventListener('click', event => {
        event.stopPropagation();
        row.classList.add('selected-row');
        row.setAttribute('aria-selected', 'true');
        onModify(packet);
      });
      detailsCell.append(select);
    }

    const selectable = activeMode === 'modify' && packet.direction === 'RX' && packet.decoded;
    row.tabIndex = 0;
    row.setAttribute('aria-selected', 'false');
    row.addEventListener('click', event => {
      if (event.target.closest('button')) return;
      row.classList.add('selected-row');
      row.setAttribute('aria-selected', 'true');
      if (selectable) onModify(packet);
      if (!event.target.closest('summary')) details.open = !details.open;
    });
    row.addEventListener('keydown', event => {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      event.preventDefault();
      row.click();
    });
  }
  node.append(table);
}
