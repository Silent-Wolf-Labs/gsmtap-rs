function addText(parent, tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

function packetDetails(packet, activeMode, onReplay, onModify) {
  const content = document.createElement('div');
  addText(content, 'code', packet.rawHex);
  if (packet.forwardStatus) addText(content, 'em', ` ${packet.forwardStatus}`);
  if (packet.parseError) addText(content, 'p', packet.parseError, 'error');
  else addText(content, 'pre', JSON.stringify(packet.decoded, null, 2));
  if (packet.modified) addText(content, 'pre', `Original: ${packet.originalRawHex}\nFinal: ${packet.finalRawHex}`);
  if (activeMode === 'modify' && packet.direction === 'RX' && packet.decoded) {
    const replay = document.createElement('button');
    replay.textContent = 'Replay';
    replay.addEventListener('click', () => onReplay(packet.id));
    content.append(replay);
    const modify = document.createElement('button');
    modify.textContent = 'Modify';
    modify.addEventListener('click', () => onModify(packet));
    content.append(modify);
  }
  return content;
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
    heading.title = columnTooltips[header];
    heading.setAttribute('aria-label', `${header}: ${columnTooltips[header]}`);
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
      const failed = packet.forwardStatus?.startsWith('error:');
      addText(row, 'td', packet.forwardStatus || 'Pending', failed ? 'error' : packet.forwardStatus ? 'success' : '');
    }
    if (showModified) addText(row, 'td', packet.modified ? 'Yes' : 'No');
    const details = document.createElement('details');
    details.dataset.packetId = packet.id;
    details.open = openPackets.has(String(packet.id));
    const summary = document.createElement('summary');
    summary.textContent = 'Expand';
    details.append(summary, packetDetails(packet, activeMode, onReplay, onModify));
    row.insertCell().append(details);
  }
  node.append(table);
}
