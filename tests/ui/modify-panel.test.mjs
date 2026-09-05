import { jest } from '@jest/globals';
import { createModifyPanel, fieldTooltips, hexFieldNames, packetFieldNames, validateModifyForm } from '../../static/components/panels/modify-panel.js';
import { createModifyPreview } from '../../static/components/modify/modify-preview.js';

function setup() {
  document.body.innerHTML = '<section id="send-section" tabindex="-1"><form id="send-form"><div id="fields"></div></form><section id="preview-card" hidden><pre id="preview"></pre><button id="confirm-send"></button><p id="result"></p></section><p id="selected-packet"></p></section>';
}

test('defines all editable numeric and hex fields', () => {
  expect(packetFieldNames).toHaveLength(12);
  expect(new Set(packetFieldNames).size).toBe(12);
  expect(hexFieldNames).toEqual(['extensionHex', 'payloadHex']);
  expect([...packetFieldNames, ...hexFieldNames].every(name => fieldTooltips[name])).toBe(true);
});

test('adds descriptions to every modify field', () => {
  setup();
  createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  for (const name of [...packetFieldNames, ...hexFieldNames]) {
  expect(document.querySelector(`[name="${name}"]`).title).toBe(fieldTooltips[name]);
  }
});

test('keeps the send action in a separate preview card', async () => {
  setup();
  const previewModification = jest.fn(async () => ({ ok: true, json: async () => ({ fieldChanges: [], originalHex: 'CA', modifiedHex: 'CB' }) }));
  const panel = createModifyPanel({ previewModification, sendModification: jest.fn(), showResult: jest.fn() });
  const packet = { id: 9, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };
  panel.selectPacket(packet);
  expect(document.querySelector('#preview-card').hidden).toBe(true);
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(document.querySelector('#preview-card').hidden).toBe(false);
  expect(document.querySelector('#preview-card #confirm-send')).not.toBeNull();
});

test('focuses the modify section when a packet is selected', () => {
  setup();
  const panel = createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const packet = { id: 11, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };
  panel.selectPacket(packet);
  expect(document.activeElement).toBe(document.querySelector('#send-section'));
});

test('disables preview button until a packet is selected', () => {
  setup();
  const panel = createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const previewButton = document.querySelector('.modify-form-actions button');
  const packet = { id: 12, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };

  expect(previewButton.disabled).toBe(true);
  panel.selectPacket(packet);
  expect(previewButton.disabled).toBe(false);
});

test('sets numeric ranges and rejects invalid field values', () => {
  setup();
  createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const form = document.querySelector('#send-form');
  expect(form.elements.arfcn.min).toBe('0');
  expect(form.elements.arfcn.max).toBe('65535');
  for (const name of packetFieldNames) form.elements[name].value = name === 'headerLengthWords' ? '4' : '1';
  form.elements.extensionHex.value = '';
  form.elements.payloadHex.value = '';
  form.elements.arfcn.value = '65536';
  expect(validateModifyForm(form)).toContain('arfcn');
  form.elements.arfcn.value = '1';
  form.elements.signalDbm.value = '1.5';
  expect(validateModifyForm(form)).toContain('signalDbm');
});

test('rejects malformed hex and mismatched extension length', () => {
  setup();
  createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const form = document.querySelector('#send-form');
  for (const name of packetFieldNames) form.elements[name].value = name === 'headerLengthWords' ? '4' : '1';
  form.elements.extensionHex.value = 'CA FE';
  form.elements.payloadHex.value = 'GG';
  expect(validateModifyForm(form)).toContain('payloadHex');
  form.elements.payloadHex.value = 'AA';
  form.elements.extensionHex.value = 'CA FE BA BE';
  form.elements.headerLengthWords.value = '4';
  expect(validateModifyForm(form)).toContain('headerLengthWords');
});

test('allows an empty optional extension', () => {
  setup();
  createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const form = document.querySelector('#send-form');
  for (const name of packetFieldNames) form.elements[name].value = name === 'headerLengthWords' ? '4' : '1';
  form.elements.extensionHex.value = '';
  form.elements.payloadHex.value = '';
  expect(validateModifyForm(form)).toBe('');
});

test('selects a packet and previews without sending', async () => {
  setup();
  const previewModification = jest.fn(async () => ({ ok: true, json: async () => ({ fieldChanges: [], originalHex: 'CA', modifiedHex: 'CB' }) }));
  const sendModification = jest.fn();
  const panel = createModifyPanel({ previewModification, sendModification, showResult: jest.fn() });
  const packet = { id: 4, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? 'CA FE BA BE' : name === 'headerLengthWords' ? 5 : 1])) };
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
  const packet = { id: 5, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? 'CA FE BA BE' : name === 'headerLengthWords' ? 5 : 1])) };
  panel.selectPacket(packet);
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(document.querySelector('#preview').textContent).toContain('arfcn: 1 → 2');
  document.querySelector('#confirm-send').click();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(sendModification).toHaveBeenCalledWith(5, expect.objectContaining({ arfcn: 1 }));
});

test('reports submit errors before calling the modification service', async () => {
  setup();
  const showResult = jest.fn();
  const previewModification = jest.fn();
  const panel = createModifyPanel({ previewModification, sendModification: jest.fn(), showResult });
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await Promise.resolve();

  expect(showResult).toHaveBeenCalledWith('Select a decoded RX packet first.');
  expect(previewModification).not.toHaveBeenCalled();

  const packet = { id: 6, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };
  panel.selectPacket(packet);
  document.querySelector('[name="payloadHex"]').value = 'GG';
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await Promise.resolve();
  expect(showResult).toHaveBeenCalledWith(expect.stringContaining('Invalid field: payloadHex'));
});

test('reports an unsuccessful preview response', async () => {
  setup();
  const showResult = jest.fn();
  const panel = createModifyPanel({
    previewModification: jest.fn(async () => ({ ok: false, text: async () => 'preview failed' })),
    sendModification: jest.fn(),
    showResult,
  });
  const packet = { id: 13, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };
  panel.selectPacket(packet);
  document.querySelector('#send-form').dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
  await new Promise(resolve => setTimeout(resolve, 0));

  expect(showResult).toHaveBeenCalledWith('preview failed');
  expect(document.querySelector('#preview-card').hidden).toBe(true);
});

test('handles a panel without a send section', () => {
  document.body.innerHTML = '<form id="send-form"><div id="fields"></div></form><section id="preview-card" hidden><pre id="preview"></pre><button id="confirm-send"></button></section><p id="selected-packet"></p>';
  const panel = createModifyPanel({ previewModification: jest.fn(), sendModification: jest.fn(), showResult: jest.fn() });
  const packet = { id: 14, decoded: Object.fromEntries([...packetFieldNames, ...hexFieldNames].map(name => [name, hexFieldNames.includes(name) ? '' : name === 'headerLengthWords' ? 4 : 1])) };

  expect(() => panel.selectPacket(packet)).not.toThrow();
});

test('renders and resets an empty modification preview', () => {
  document.body.innerHTML = '<section id="preview-card" hidden><pre id="preview"></pre><button id="confirm-send"></button></section>';
  const onConfirm = jest.fn();
  const preview = createModifyPreview({ onConfirm });

  preview.show({ fieldChanges: [], originalHex: 'CA', modifiedHex: 'CB' });
  expect(document.querySelector('#preview').textContent).toContain('No field changes.');
  expect(document.querySelector('#confirm-send').disabled).toBe(false);
  document.querySelector('#confirm-send').click();
  expect(onConfirm).toHaveBeenCalled();
  preview.reset();
  expect(document.querySelector('#preview-card').hidden).toBe(true);
  expect(document.querySelector('#confirm-send').disabled).toBe(true);
});
