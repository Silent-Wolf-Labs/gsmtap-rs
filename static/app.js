const names = ['version', 'headerLengthWords', 'messageType', 'timeslot', 'arfcn', 'signalDbm', 'snrDb', 'frameNumber', 'subtype', 'antennaNumber', 'subSlot', 'reserved'];
const fields = document.querySelector('#fields');
const packetsNode = document.querySelector('#packets');
const form = document.querySelector('#send-form');
const previewNode = document.querySelector('#preview');
const resultNode = document.querySelector('#result');
const confirmButton = document.querySelector('#confirm-send');
const selectedNode = document.querySelector('#selected-packet');
let activeMode = 'listen';
let packets = [];
let selectedPacket = null;
let pendingPayload = null;

for (const name of names) {
  const label = document.createElement('label');
  label.textContent = name;
  const input = document.createElement('input');
  input.name = name;
  input.type = 'number';
  label.append(input);
  fields.append(label);
}

function showResult(text) { resultNode.textContent = text; }

function filteredPackets() {
  const direction = document.querySelector('#filter-direction').value;
  const parse = document.querySelector('#filter-parse').value;
  const modified = document.querySelector('#filter-modified').value;
  return packets.filter(packet =>
    (direction === 'all' || packet.direction === direction) &&
    (parse === 'all' || (parse === 'success') === !packet.parseError) &&
    (modified === 'all' || (modified === 'yes') === packet.modified));
}

function addText(parent, tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  parent.append(node);
  return node;
}

function render() {
  packetsNode.replaceChildren();
  for (const packet of filteredPackets().slice().reverse()) {
    const article = document.createElement('article');
    addText(article, 'strong', `${packet.direction} #${packet.id} `);
    addText(article, 'span', `${new Date(Number(packet.timestampMs)).toLocaleString()} — ${packet.peer}`);
    article.append(document.createElement('br'));
    addText(article, 'code', packet.rawHex);
    if (packet.forwardStatus) addText(article, 'em', ` ${packet.forwardStatus}`);
    if (packet.parseError) addText(article, 'p', packet.parseError, 'error');
    else addText(article, 'pre', JSON.stringify(packet.decoded, null, 2));
    if (packet.modified) addText(article, 'pre', `Original: ${packet.originalRawHex}\nFinal: ${packet.finalRawHex}`);
    if (activeMode === 'modify' && packet.direction === 'RX' && packet.decoded) {
      const replay = document.createElement('button');
      replay.textContent = 'Replay';
      replay.addEventListener('click', () => replayPacket(packet.id));
      article.append(replay);
      const modify = document.createElement('button');
      modify.textContent = 'Modify';
      modify.addEventListener('click', () => selectPacket(packet));
      article.append(modify);
    }
    packetsNode.append(article);
  }
}

function selectPacket(packet) {
  selectedPacket = packet;
  pendingPayload = null;
  confirmButton.disabled = true;
  previewNode.textContent = '';
  selectedNode.textContent = `Selected RX packet #${packet.id}. Preview before sending.`;
  for (const name of names) form.elements[name].value = packet.decoded[name];
  form.elements.extensionHex.value = packet.decoded.extensionHex;
  form.elements.payloadHex.value = packet.decoded.payloadHex;
}

function payloadFromForm() {
  const data = Object.fromEntries(new FormData(form));
  for (const name of names) data[name] = Number(data[name]);
  return data;
}

async function replayPacket(id) {
  const response = await fetch(`/api/packets/${id}/replay`, { method: 'POST' });
  showResult(await response.text());
}

form.addEventListener('submit', async event => {
  event.preventDefault();
  if (!selectedPacket) return showResult('Select a decoded RX packet first.');
  const payload = payloadFromForm();
  const response = await fetch(`/api/packets/${selectedPacket.id}/modify-preview`, {
    method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(payload),
  });
  if (!response.ok) return showResult(await response.text());
  const preview = await response.json();
  pendingPayload = payload;
  confirmButton.disabled = false;
  const changes = preview.fieldChanges.map(change => `${change.field}: ${change.original} → ${change.modified}`).join('\n') || 'No field changes.';
  previewNode.textContent = `Original:\n${preview.originalHex}\n\nModified:\n${preview.modifiedHex}\n\nChanges:\n${changes}`;
});

confirmButton.addEventListener('click', async () => {
  if (!selectedPacket || !pendingPayload) return;
  const response = await fetch(`/api/packets/${selectedPacket.id}/modify-send`, {
    method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify(pendingPayload),
  });
  showResult(await response.text());
  confirmButton.disabled = true;
  pendingPayload = null;
});

for (const filter of document.querySelectorAll('#filters select')) filter.addEventListener('change', render);
fetch('/api/status').then(response => response.json()).then(status => {
  activeMode = status.mode;
  document.querySelector('#status').textContent = `MODE: ${status.mode.toUpperCase()} · RX ${status.gsmtapListen} · FORWARD ${status.gsmtapForward || 'disabled'}`;
  document.querySelector('#send-section').hidden = activeMode !== 'modify';
  refreshPackets();
});
function refreshPackets() { fetch('/api/packets').then(response => response.json()).then(data => { packets = data; render(); }); }
const events = new EventSource('/api/events');
events.onmessage = refreshPackets;
