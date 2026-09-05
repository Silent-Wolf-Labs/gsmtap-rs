export function createModifyPreview({ documentRef = document, previewCard = documentRef.querySelector('#preview-card'), previewNode = documentRef.querySelector('#preview'), confirmButton = documentRef.querySelector('#confirm-send'), onConfirm }) {
  confirmButton.addEventListener('click', onConfirm);
  return {
    reset() { confirmButton.disabled = true; previewCard.hidden = true; previewNode.textContent = ''; },
    show(preview) {
      const changes = preview.fieldChanges.map(change => `${change.field}: ${change.original} → ${change.modified}`).join('\n') || 'No field changes.';
      previewNode.textContent = `Original:\n${preview.originalHex}\n\nModified:\n${preview.modifiedHex}\n\nChanges:\n${changes}`;
      confirmButton.disabled = false;
      previewCard.hidden = false;
    },
    setConfirmEnabled(enabled) { confirmButton.disabled = !enabled; },
  };
}
