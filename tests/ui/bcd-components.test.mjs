import { readFileSync } from 'node:fs';
import { jest } from '@jest/globals';
import { createBcdResult } from '../../static/components/conversion/bcd-result.js';
import { ConversionOutputModel } from '../../static/models/conversion/output-model.js';
import { createConversionController } from '../../static/controllers/conversion-controller.js';
import { createConversionPanels } from '../../static/components/conversion/conversion-panel.js';

function resultView(result) {
  const view = createBcdResult(); const model = new ConversionOutputModel();
  model.setResult(result); view.update(model); return { view, model };
}

test('scalar results show values and characters without a destination table', () => {
  const { view, model } = resultView({ operation: 'bcd2char', success: true, returnValue: 65, outputHex: '41', outputText: 'A', initialDestinationHex: '', finalDestinationHex: '' });
  expect(view.node.textContent).toContain('Character byte value: 65');
  expect(view.node.querySelector('[aria-label="BCD decoded text"]').textContent).toContain('A');
  expect(view.node.querySelector('.conversion-buffer').hidden).toBe(true);
  model.setResult({ operation: 'char2bcd', success: true, returnValue: 0, outputHex: '00', outputText: null });
  view.update(model); expect(view.node.textContent).toContain('Nibble value: 0');
});

test('decode reports successful truncation without overstating actual output', () => {
  const { view } = resultView({ operation: 'bcd2str', success: true, returnValue: 10, outputHex: '31 32', outputText: '12', initialDestinationHex: 'AA AA AA', finalDestinationHex: '31 32 00' });
  expect(view.node.textContent).toContain('Requested digits: 10; displayed: 2');
  expect(view.node.textContent).toContain('Output truncated');
  expect(view.node.querySelectorAll('tbody tr')).toHaveLength(3);
});

test('errors retain partial text and copyable final buffers', () => {
  const { view, model } = resultView({ operation: 'bcd2str', success: false, returnValue: null, outputHex: '31 41', outputText: '1A', error: { message: 'Disallowed digit' }, initialDestinationHex: 'AA AA AA AA', finalDestinationHex: '31 41 00 AA' });
  expect(view.node.querySelector('.error').textContent).toBe('Disallowed digit');
  expect(view.node.querySelector('[aria-label="BCD decoded text"]').textContent).toContain('partial or disallowed');
  expect([...view.node.querySelectorAll('tbody tr')].at(-1).cells[3].textContent).toBe('No');
  const finalCopy = [...view.node.querySelectorAll('button')].find(button => button.textContent === 'Copy final buffer');
  expect(finalCopy.disabled).toBe(false);
  model.reset(); view.update(model); expect(view.node.textContent).toBe('No conversion result.');
});

test('encoding identifies full buffer and returned position without claiming bytes written', () => {
  const { view } = resultView({ operation: 'str2bcd', success: true, returnValue: 0, outputHex: 'A1 AA', initialDestinationHex: 'AA AA', finalDestinationHex: 'A1 AA' });
  expect(view.node.textContent).toContain('Returned used-byte position: 0');
  expect(view.node.textContent).toContain('Complete BCD destination: A1 AA');
});

test('BCD forms submit operation-specific requests and display decoded results', async () => {
  document.documentElement.innerHTML = readFileSync(new URL('../../static/index.html', import.meta.url), 'utf8');
  const service = { available: true, convert: jest.fn(async () => ({ operation: 'bcd2str', success: true, returnValue: 2, outputText: '12', outputHex: '31 32', initialDestinationHex: 'AA AA AA', finalDestinationHex: '31 32 00' })) };
  let views;
  const controller = createConversionController({ service, onChange: (tool,state) => views.get(tool).update(state) });
  views = createConversionPanels({ controller });
  controller.update('bcd','operation','bcd2str'); controller.update('bcd','sourceEncoding','hex');
  controller.update('bcd','source','21'); controller.update('bcd','endNibble','2');
  controller.update('bcd','destination.capacity','3');
  await controller.submit('bcd');
  expect(service.convert).toHaveBeenCalledWith('bcd', expect.objectContaining({ operation:'bcd2str', source:{encoding:'hex',data:'21'}, options:{startNibble:0,endNibble:2,allowHex:false} }), expect.anything());
  const panel = document.querySelector('[data-workbench-view="bcd"]');
  expect(panel.querySelector('.conversion-result').textContent).toContain('Decoded text: 12');
  controller.update('bcd','operation','char2bcd');
  expect(panel.querySelectorAll('fieldset')[1].hidden).toBe(true);
  expect(panel.querySelector('.conversion-result').textContent).toBe('No conversion result.');
});
