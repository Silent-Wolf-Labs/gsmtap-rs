export function createCopyOutput({ documentRef = document, label, writeText = text => navigator.clipboard.writeText(text) }) {
  const node = documentRef.createElement('span');
  const button = documentRef.createElement('button'); button.type = 'button'; button.textContent = label;
  const message = documentRef.createElement('span'); message.setAttribute('role', 'status'); message.setAttribute('aria-live', 'polite');
  node.append(button, message);
  let value = null, revision = 0, pending = false;
  button.addEventListener('click', async () => {
    if (value === null || pending) return;
    pending = true; button.disabled = true;
    const requestedRevision = revision;
    try {
      await writeText(value);
      if (requestedRevision === revision) message.textContent = 'Copied.';
    } catch {
      if (requestedRevision === revision) message.textContent = 'Unable to copy. Select and copy the output manually.';
    } finally {
      if (requestedRevision === revision) { pending = false; button.disabled = value === null; }
    }
  });
  return { node, update(text) {
    revision++; value = text; pending = false;
    message.textContent = ''; button.disabled = text === null;
  } };
}
