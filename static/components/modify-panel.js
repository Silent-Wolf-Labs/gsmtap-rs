export const packetFieldNames = ['version', 'headerLengthWords', 'messageType', 'timeslot', 'arfcn', 'signalDbm', 'snrDb', 'frameNumber', 'subtype', 'antennaNumber', 'subSlot', 'reserved'];
export const hexFieldNames = ['extensionHex', 'payloadHex'];

export function createModifyPanel({ previewModification, sendModification, showResult }) {
  const fields = document.querySelector('#fields');
  const form = document.querySelector('#send-form');
  const previewNode = document.querySelector('#preview');
  const confirmButton = document.querySelector('#confirm-send');
  const selectedNode = document.querySelector('#selected-packet');
  let selectedPacket = null;
  let pendingPayload = null;

  for (const name of packetFieldNames) {
    const label = document.createElement('label');
    label.textContent = name;
    const input = document.createElement('input');
    input.name = name;
    input.type = 'number';
    label.append(input);
    fields.append(label);
  }

  function selectPacket(packet) {
    selectedPacket = packet;
    pendingPayload = null;
    confirmButton.disabled = true;
    previewNode.textContent = '';
    selectedNode.textContent = `Selected RX packet #${packet.id}. Preview before sending.`;
    for (const name of packetFieldNames) form.elements[name].value = packet.decoded[name];
    form.elements[hexFieldNames[0]].value = packet.decoded.extensionHex;
    form.elements[hexFieldNames[1]].value = packet.decoded.payloadHex;
  }

  form.addEventListener('submit', async event => {
    event.preventDefault();
    if (!selectedPacket) return showResult('Select a decoded RX packet first.');
    const payload = Object.fromEntries(new FormData(form));
    for (const name of packetFieldNames) payload[name] = Number(payload[name]);
    const response = await previewModification(selectedPacket.id, payload);
    if (!response.ok) return showResult(await response.text());
    const preview = await response.json();
    pendingPayload = payload;
    confirmButton.disabled = false;
    const changes = preview.fieldChanges.map(change => `${change.field}: ${change.original} → ${change.modified}`).join('\n') || 'No field changes.';
    previewNode.textContent = `Original:\n${preview.originalHex}\n\nModified:\n${preview.modifiedHex}\n\nChanges:\n${changes}`;
  });

  confirmButton.addEventListener('click', async () => {
    if (!selectedPacket || !pendingPayload) return;
    const response = await sendModification(selectedPacket.id, pendingPayload);
    showResult(await response.text());
    confirmButton.disabled = true;
    pendingPayload = null;
  });

  return { selectPacket };
}
