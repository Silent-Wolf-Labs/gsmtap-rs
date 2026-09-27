const CLEAR_HISTORY_PREFERENCE_KEY = 'gsmtap.clear-history.skip-confirmation';

export function createClearHistoryDialog({ documentRef = document, onConfirm }) {
  const dialogNode = documentRef.querySelector('#clear-history-dialog');
  const skipNode = documentRef.querySelector('#clear-history-skip');
  const confirmNode = documentRef.querySelector('#confirm-clear-history');
  const cancelNode = documentRef.querySelector('#cancel-clear-history');

  function close() {
    if (typeof dialogNode?.close === 'function') dialogNode.close();
    else dialogNode?.removeAttribute('open');
  }

  function hasSkippedConfirmation() {
    try {
      return documentRef.defaultView?.localStorage.getItem(CLEAR_HISTORY_PREFERENCE_KEY) === 'true';
    } catch {
      return false;
    }
  }

  function rememberSkipPreference() {
    if (!skipNode?.checked) return;
    try {
      documentRef.defaultView?.localStorage.setItem(CLEAR_HISTORY_PREFERENCE_KEY, 'true');
    } catch {
      // Storage may be unavailable.
    }
  }

  function open() {
    if (hasSkippedConfirmation()) {
      void onConfirm?.();
      return;
    }
    if (skipNode) skipNode.checked = false;
    if (typeof dialogNode?.showModal === 'function') dialogNode.showModal();
    else dialogNode?.setAttribute('open', '');
  }

  function confirm(event) {
    event.preventDefault();
    rememberSkipPreference();
    close();
    void onConfirm?.();
  }

  function cancel(event) {
    event.preventDefault();
    close();
  }

  confirmNode?.addEventListener('click', confirm);
  cancelNode?.addEventListener('click', cancel);

  return { open, close };
}