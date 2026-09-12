import { fieldTooltips, hexFieldNames, packetFieldNames } from '../modify/modify-fields.js';
import { createModifyForm } from '../modify/modify-form.js';
import { createModifyPreview } from '../modify/modify-preview.js';
import { ModifyModel } from '../../models/modify-model.js';
import { readModifyForm, validateModifyForm } from '../modify/modify-validation.js';
import { createModificationService } from '../../services/modification-service.js';

export { fieldTooltips, hexFieldNames, packetFieldNames, readModifyForm, validateModifyForm };

export function createModifyPanel({ documentRef = document, previewModification, sendModification, showResult, model = new ModifyModel() }) {
  const form = documentRef.querySelector('#send-form');
  const previewCard = documentRef.querySelector('#preview-card');
  const sendSection = documentRef.querySelector('#send-section');
  const selectedNode = documentRef.querySelector('#selected-packet');
  const service = createModificationService({ previewModification, sendModification });
  const formView = createModifyForm({ documentRef, form, onSubmit: handleSubmit, onFieldChange: (key, value) => model.updateField(key, value) });
  const previewView = createModifyPreview({ documentRef, previewCard, onConfirm: handleConfirm });
  model.subscribe((currentModel, reason) => {
    formView.setPreviewEnabled(currentModel.hasChanges);
    if (reason === 'field' && !currentModel.pendingPayload) previewView.reset();
  });
  previewView.reset();

  function selectPacket(packet) {
    model.selectPacket(packet);
    showResult('');
    formView.setValues(model.values);
    previewView.reset();
    selectedNode.textContent = `Selected RX packet #${packet.id}. Preview before sending.`;
    if (sendSection) {
      sendSection.focus();
      sendSection.scrollIntoView?.({ behavior: 'smooth', block: 'start' });
    }
  }

  function resetSelection() {
    model.reset();
    formView.reset();
    previewView.reset();
    selectedNode.textContent = 'Select a decoded RX packet to edit it.';
  }

  async function handleSubmit(event) {
    event.preventDefault();
    if (!model.selectedPacket) return showResult('Select a decoded RX packet first.');
    if (!model.hasChanges) return;
    const validationError = validateModifyForm(form);
    if (validationError) return showResult(`Invalid field: ${validationError}`);
    const payload = model.toRequestPayload();
    const response = await service.preview(model.selectedPacket.id, payload);
    if (!response.ok) return showResult(await response.text());
    model.setPendingPayload(payload);
    previewView.show(await response.json());
  }

  async function handleConfirm() {
    if (!model.selectedPacket || !model.pendingPayload) return;
    const response = await service.send(model.selectedPacket.id, model.pendingPayload);
    if (!response.ok) {
      showResult(await response.text());
      return;
    }
    showResult('Packet modified and sent successfully.');
    resetSelection();
  }

  return { selectPacket, resetSelection };
}

export const modifyCapability = Object.freeze({
  mode: 'modify',
  inspect: true,
  modify: true,
  replay: false,
  forwarding: false,
  actions: Object.freeze({}),
});
