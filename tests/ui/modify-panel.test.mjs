import { jest } from '@jest/globals';
import { createModifyPanel, hexFieldNames, packetFieldNames } from '../../static/components/modify-panel.js';

function setup() {
  document.body.innerHTML = '<form id="send-form"><div id="fields"></div><textarea name="extensionHex"></textarea><textarea name="payloadHex"></textarea><button id="confirm-send"></button></form><pre id="preview"></pre><pre id="result"></pre><p id="selected-packet"></p>';
}

test('defines all editable numeric and hex fields', () => {
  expect(packetFieldNames).toHaveLength(12);
  expect(new Set(packetFieldNames).size).toBe(12);
  expect(hexFieldNames).toEqual(['extensionHex', 'payloadHex']);
});

test('selects a packet and previews without sending', async () => {
  setup();
  const previewModification = jest.fn(async () => ({ ok: true, json: async () => ({ fieldChanges: [], originalHex: 'CA', modifiedHex: 'CB' }) }));
  const sendModification = jest.fn();
  const panel = createModifyPanel({ previewModification, sendModification, showResult: jest.fn() });
  const packet = { id: 4, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, 1])) };
  panel.selectPacket(packet);
  expect(document.querySelector('#selected-packet').textContent).toContain('#4');
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await Promise.resolve();
  expect(previewModification).toHaveBeenCalledWith(4, expect.objectContaining({ arfcn: 1 }));
  expect(sendModification).not.toHaveBeenCalled();
});

test('shows preview changes and sends after confirmation', async () => {
  setup();
  const previewModification = jest.fn(async () => ({ ok: true, json: async () => ({ fieldChanges: [{ field: 'arfcn', original: 1, modified: 2 }], originalHex: 'CA', modifiedHex: 'CB' }) }));
  const sendModification = jest.fn(async () => ({ text: async () => 'sent' }));
  const panel = createModifyPanel({ previewModification, sendModification, showResult: jest.fn() });
  const packet = { id: 5, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, 1])) };
  panel.selectPacket(packet);
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(document.querySelector('#preview').textContent).toContain('arfcn: 1 → 2');
  document.querySelector('#confirm-send').click();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(sendModification).toHaveBeenCalledWith(5, expect.objectContaining({ arfcn: 1 }));
});
