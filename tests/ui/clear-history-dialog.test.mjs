import { jest } from '@jest/globals';
import { createClearHistoryDialog } from '../../static/components/dialogs/clear-history-dialog.js';

function setup() {
  document.body.innerHTML = `
    <button id="clear-history" type="button">Clear history</button>
    <dialog id="clear-history-dialog">
      <form method="dialog">
        <input id="clear-history-skip" type="checkbox">
        <button id="cancel-clear-history" type="submit">Cancel</button>
        <button id="confirm-clear-history" type="submit">Clear history</button>
      </form>
    </dialog>
  `;
  localStorage.clear();
}

beforeEach(() => {
  setup();
});

afterEach(() => {
  jest.restoreAllMocks();
});

test('requires confirmation before invoking the callback', () => {
  const onConfirm = jest.fn();
  const dialog = createClearHistoryDialog({ documentRef: document, onConfirm });

  dialog.open();

  expect(onConfirm).not.toHaveBeenCalled();
  expect(document.querySelector('#clear-history-dialog').hasAttribute('open')).toBe(true);
});

test('invokes confirmation and stores the skip preference', () => {
  const onConfirm = jest.fn();
  createClearHistoryDialog({ documentRef: document, onConfirm }).open();
  document.querySelector('#clear-history-skip').checked = true;

  document.querySelector('#confirm-clear-history').click();

  expect(onConfirm).toHaveBeenCalledTimes(1);
  expect(localStorage.getItem('gsmtap.clear-history.skip-confirmation')).toBe('true');
  expect(document.querySelector('#clear-history-dialog').hasAttribute('open')).toBe(false);
});

test('cancels without invoking the callback', () => {
  const onConfirm = jest.fn();
  createClearHistoryDialog({ documentRef: document, onConfirm }).open();

  document.querySelector('#cancel-clear-history').click();

  expect(onConfirm).not.toHaveBeenCalled();
  expect(document.querySelector('#clear-history-dialog').hasAttribute('open')).toBe(false);
});

test('skips confirmation when the preference is persisted', () => {
  localStorage.setItem('gsmtap.clear-history.skip-confirmation', 'true');
  const onConfirm = jest.fn();
  const dialog = createClearHistoryDialog({ documentRef: document, onConfirm });

  dialog.open();

  expect(onConfirm).toHaveBeenCalledTimes(1);
  expect(document.querySelector('#clear-history-dialog').hasAttribute('open')).toBe(false);
});

test('falls back to the open attribute when showModal is unavailable', () => {
  const onConfirm = jest.fn();
  const dialogNode = document.querySelector('#clear-history-dialog');
  dialogNode.showModal = undefined;
  const dialog = createClearHistoryDialog({ documentRef: document, onConfirm });

  dialog.open();

  expect(dialogNode.hasAttribute('open')).toBe(true);
});

test('uses showModal when it is available', () => {
  const onConfirm = jest.fn();
  const dialogNode = document.querySelector('#clear-history-dialog');
  dialogNode.showModal = jest.fn();
  const dialog = createClearHistoryDialog({ documentRef: document, onConfirm });

  dialog.open();

  expect(dialogNode.showModal).toHaveBeenCalledTimes(1);
});

test('continues when storage is unavailable', () => {
  jest.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
    throw new Error('storage unavailable');
  });
  jest.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
    throw new Error('storage unavailable');
  });
  const onConfirm = jest.fn();
  const dialog = createClearHistoryDialog({ documentRef: document, onConfirm });

  dialog.open();
  document.querySelector('#clear-history-skip').checked = true;
  document.querySelector('#confirm-clear-history').click();

  expect(onConfirm).toHaveBeenCalledTimes(1);
});