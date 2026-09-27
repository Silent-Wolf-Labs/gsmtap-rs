import { packetSourceAddress, relayForwardingState } from '../../models/packet-model.js';
import { renderPacketDetails } from './packet-schema.js';
import { createColumnFilter } from './column-filter.js';
import { filterDefinition, visibleFilterIds } from '../filters.js';
import { applyTooltip } from '../tooltip.js';

function addText(parent, tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

export function tableColumns(activeMode) {
  const columns = [];
  if (activeMode === 'relay') columns.push('Select');
  columns.push('Packet', 'Direction', 'Timestamp', 'Source address', 'Decode');
  if (activeMode === 'relay') columns.push('Forward');
  if (activeMode === 'modify') columns.push('Modified');
  return columns;
}

const columnTooltips = {
  Select: 'Select packets for batch forwarding.',
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
  selectedPacketIds = new Set(),
  onSelectionChange,
  onSelectAllChange,
  filters = {},
  onFilterChange,
  openFilter = null,
  onFilterOpenChange,
  hasPackets = packets.length > 0,
} = {}) {
  const previousOriginalInput = node.querySelector('.selected-row + .packet-details-row .packet-original-input');
  const previousOriginalInputOpen = previousOriginalInput?.open ?? false;
  node.replaceChildren();
  if (!packets.length) {
    if (selectedPacketId !== null) onSelectPacket?.(null);
    addText(node, 'p', hasPackets ? 'No packets match the current filters.' : 'No packets captured yet.', 'empty-state');
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
    const heading = document.createElement('th');
    heading.className = 'packet-column-header';
    if (header === 'Select') {
      heading.classList.add('packet-column-select');
      const selectAllCheckbox = document.createElement('input');
      selectAllCheckbox.type = 'checkbox';
      selectAllCheckbox.className = 'packet-select-all';
      selectAllCheckbox.setAttribute('aria-label', 'Select all visible packets');
      const visibleIds = packets.map(p => p.id);
      const selectedCount = visibleIds.filter(id => selectedPacketIds?.has(id)).length;
      if (selectedCount === visibleIds.length && visibleIds.length > 0) {
        selectAllCheckbox.checked = true;
        selectAllCheckbox.indeterminate = false;
      } else if (selectedCount > 0) {
        selectAllCheckbox.checked = false;
        selectAllCheckbox.indeterminate = true;
      } else {
        selectAllCheckbox.checked = false;
        selectAllCheckbox.indeterminate = false;
      }
      selectAllCheckbox.addEventListener('click', event => event.stopPropagation());
      selectAllCheckbox.addEventListener('change', event => {
        event.stopPropagation();
        onSelectAllChange?.(visibleIds, selectAllCheckbox.checked);
      });
      heading.append(selectAllCheckbox);
    } else {
      addText(heading, 'span', header, 'packet-column-header-label');
      const field = ({ Packet: 'packet', Direction: 'direction', Timestamp: 'timestamp', Decode: 'decode', Modified: 'modified', 'Source address': 'sourceAddress' })[header];
      if (field && visibleFilterIds(activeMode).includes(field) && onFilterChange) {
        const definition = filterDefinition(field);
        const currentValue = filters[field];
        const active = typeof currentValue === 'string'
          ? Boolean(currentValue.trim()) : currentValue !== null && currentValue !== undefined;
        const filter = createColumnFilter({
          label: header,
          field,
          value: currentValue,
          options: definition.options,
          text: !definition.options,
          open: openFilter === field,
          onOpenChange: isOpen => onFilterOpenChange?.(isOpen ? field : null),
          onChange: value => onFilterChange({ field, value: value || null }),
        });
        filter.trigger.classList.toggle('active', active);
        filter.trigger.setAttribute('aria-label', active ? `Filter ${header}, active` : `Filter ${header}`);
        heading.append(filter.node);
      }
    }
    head.append(heading);
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
    if (activeMode === 'relay') {
      const selectCell = row.insertCell();
      selectCell.className = 'packet-select-cell';
      const rowCheckbox = document.createElement('input');
      rowCheckbox.type = 'checkbox';
      rowCheckbox.className = 'packet-row-checkbox';
      rowCheckbox.setAttribute('aria-label', `Select packet ${packet.id}`);
      rowCheckbox.checked = Boolean(selectedPacketIds?.has(packet.id));
      rowCheckbox.addEventListener('click', event => {
        event.stopPropagation();
      });
      rowCheckbox.addEventListener('change', event => {
        event.stopPropagation();
        onSelectionChange?.(packet.id, rowCheckbox.checked);
      });
      selectCell.append(rowCheckbox);
    }
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
      if (event.target.closest('button') || event.target.closest('input[type="checkbox"]')) return;
      if (expandedPacketId === packet.id) collapsePacket();
      else expandPacket(row, packet);
    });
    row.addEventListener('keydown', event => {
      if (event.key !== 'Enter' && event.key !== ' ') return;
      if (event.target.closest('input[type="checkbox"]')) return;
      event.preventDefault();
      row.click();
    });
  }
  node.append(table);
}
