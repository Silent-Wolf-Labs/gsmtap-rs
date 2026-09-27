import { editableFieldLabels, editableHeaderFields, displayHeaderValue } from '../packet/packet-schema.js';

export function createModifyPreview({ documentRef = document, previewCard = documentRef.querySelector('#preview-card'), previewNode = documentRef.querySelector('#preview'), confirmButton = documentRef.querySelector('#confirm-send'), onConfirm }) {
  confirmButton.addEventListener('click', onConfirm);
  function renderValue(parent, value, preserveWhitespace) {
    if (preserveWhitespace) {
      const node = documentRef.createElement('pre');
      node.textContent = value ?? '—';
      parent.append(node);
    } else parent.textContent = value == null ? '—' : String(value);
  }
  function renderRow(tableBody, label, original, modified, changed, preserveWhitespace = false) {
    const row = tableBody.insertRow();
    if (changed) row.className = 'modify-preview-changed';
    const field = row.insertCell();
    field.textContent = label;
    renderValue(row.insertCell(), original, preserveWhitespace);
    renderValue(row.insertCell(), modified, preserveWhitespace);
  }
  return {
    reset() { confirmButton.disabled = true; previewCard.hidden = true; previewNode.replaceChildren(); },
    show(preview) {
      previewNode.replaceChildren();
      const changed = new Set((preview.fieldChanges || []).map(change => change.field));
      const table = documentRef.createElement('table');
      table.className = 'modify-comparison-table';
      const head = table.createTHead().insertRow();
      for (const label of ['Field', 'Original', 'Modified']) {
        const cell = documentRef.createElement('th');
        cell.scope = 'col';
        cell.textContent = label;
        head.append(cell);
      }
      const body = table.createTBody();
      for (const [label, name] of editableHeaderFields) renderRow(body, label, displayHeaderValue(preview.originalDecoded, name), displayHeaderValue(preview.modifiedDecoded, name), changed.has(name));
      for (const name of ['extensionHex', 'payloadHex']) renderRow(body, editableFieldLabels[name], preview.originalDecoded?.[name], preview.modifiedDecoded?.[name], changed.has(name), true);
      previewNode.append(table);
      confirmButton.disabled = false;
      previewCard.hidden = false;
    },
    setConfirmEnabled(enabled) { confirmButton.disabled = !enabled; },
  };
}
