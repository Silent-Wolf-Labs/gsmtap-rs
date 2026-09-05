import { fieldTooltips, hexFieldNames, packetFieldNames } from '../modify/modify-fields.js';
import { createModifyForm } from '../modify/modify-form.js';
import { createModifyPreview } from '../modify/modify-preview.js';
import { createModifyState, selectModifyPacket, setPendingPayload } from '../modify/modify-state.js';
import { readModifyForm, toModifyPayload, validateModifyForm } from '../modify/modify-validation.js';
import { createModificationService } from '../../services/modification-service.js';

export { fieldTooltips, hexFieldNames, packetFieldNames, readModifyForm, validateModifyForm };

export function createModifyPanel({ documentRef = document, previewModification, sendModification, showResult }) {
  const form = documentRef.querySelector('#send-form');
  const previewCard = documentRef.querySelector('#preview-card');
  const sendSection = documentRef.querySelector('#send-section');
  const selectedNode = documentRef.querySelector('#selected-packet');
  let state = createModifyState();
  const service = createModificationService({ previewModification, sendModification });
  const formView = createModifyForm({ documentRef, form, onSubmit: handleSubmit });
  const previewView = createModifyPreview({ documentRef, previewCard, onConfirm: handleConfirm });
  previewView.reset();

  function selectPacket(packet) {
    state = selectModifyPacket(state, packet);
    formView.setPreviewEnabled(true);
    formView.setValues(packet.decoded);
    previewView.reset();
    selectedNode.textContent = `Selected RX packet #${packet.id}. Preview before sending.`;
    if (sendSection) {
      sendSection.focus();
      sendSection.scrollIntoView?.({ behavior: 'smooth', block: 'start' });
    }
  }

  function resetSelection() {
    state = createModifyState();
    formView.reset();
    previewView.reset();
    selectedNode.textContent = 'Select a decoded RX packet to edit it.';
  }

  async function handleSubmit(event) {
    event.preventDefault();
    if (!state.selectedPacket) return showResult('Select a decoded RX packet first.');
    const validationError = validateModifyForm(form);
    if (validationError) return showResult(`Invalid field: ${validationError}`);
    const payload = toModifyPayload(formView.readValues());
    const response = await service.preview(state.selectedPacket.id, payload);
    if (!response.ok) return showResult(await response.text());
    state = setPendingPayload(state, payload);
    previewView.show(await response.json());
  }

  async function handleConfirm() {
    if (!state.selectedPacket || !state.pendingPayload) return;
    const response = await service.send(state.selectedPacket.id, state.pendingPayload);
    if (!response.ok) {
      showResult(await response.text());
      return;
    }
    showResult('Packet modified and sent successfully.');
    resetSelection();
  }

  return { selectPacket };
}

export const modifyCapability = Object.freeze({
  mode: 'modify',
  inspect: true,
  modify: true,
  replay: false,
  forwarding: false,
  actions: Object.freeze({}),
});
