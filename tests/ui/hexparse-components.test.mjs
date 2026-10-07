import { jest } from '@jest/globals';
import { createBufferTable } from '../../static/components/conversion/buffer-table.js';
import { createCopyOutput } from '../../static/components/conversion/copy-output.js';
import { createHexparseResult } from '../../static/components/conversion/hexparse-result.js';
import { ConversionOutputModel } from '../../static/models/conversion/output-model.js';
import { createConversionService } from '../../static/services/conversion-service.js';

test('buffer table marks changed bytes and retains untouched suffixes', () => {
  const table = createBufferTable(); table.update('AA BB CC', '12 A0 CC');
  const rows = [...table.node.querySelectorAll('tbody tr')];
  expect(rows.map(row => [...row.cells].map(cell => cell.textContent))).toEqual([
    ['0', 'AA', '12', 'Yes'], ['1', 'BB', 'A0', 'Yes'], ['2', 'CC', 'CC', 'No'],
  ]);
  expect(rows.filter(row => row.classList.contains('conversion-byte-changed'))).toHaveLength(2);
  expect(table.node.querySelector('caption').textContent).toBe('Complete destination buffer');
});

test('large buffers page without creating thousands of rows or losing bytes', () => {
  const table = createBufferTable(); table.update('AA '.repeat(260), 'BB '.repeat(260));
  const [previous, next] = table.node.querySelectorAll('button');
  expect(table.node.querySelectorAll('tbody tr')).toHaveLength(128);
  expect(previous.disabled).toBe(true);
  next.click(); expect(table.node.querySelector('tbody td').textContent).toBe('128');
  next.click(); expect(table.node.querySelectorAll('tbody tr')).toHaveLength(4);
  expect(next.disabled).toBe(true);
  previous.click(); expect(table.node.querySelector('tbody td').textContent).toBe('128');
  table.update('', ''); expect(table.node.querySelectorAll('tbody tr')).toHaveLength(0);
});

test('copy controls support empty output and report clipboard failures', async () => {
  const writeText = jest.fn(async () => {});
  const copy = createCopyOutput({ label: 'Copy', writeText });
  copy.update(''); copy.node.querySelector('button').click(); await Promise.resolve();
  expect(writeText).toHaveBeenCalledWith('');
  expect(copy.node.textContent).toContain('Copied.');
  writeText.mockRejectedValueOnce(new Error('Denied'));
  copy.update('CA'); copy.node.querySelector('button').click(); await Promise.resolve();
  expect(copy.node.textContent).toContain('Unable to copy');
  copy.update(null); expect(copy.node.querySelector('button').disabled).toBe(true);
});

test('copy completion does not revive feedback after reset', async () => {
  let resolve;
  const copy = createCopyOutput({ label: 'Copy', writeText: () => new Promise(done => { resolve = done; }) });
  copy.update('CA'); copy.node.querySelector('button').click(); copy.update(null);
  resolve(); await Promise.resolve();
  expect(copy.node.textContent).not.toContain('Copied.');
  expect(copy.node.querySelector('button').disabled).toBe(true);
});

test('failure result shows complete partial writes and only allows buffer copying', () => {
  const result = createHexparseResult(); const model = new ConversionOutputModel();
  result.update(model);
  expect(result.node.querySelector('.conversion-buffer').hidden).toBe(true);
  model.setResult({ operation: 'parse', success: false, returnValue: null, outputHex: null,
    initialDestinationHex: 'AA AA AA', finalDestinationHex: '12 A0 00', error: { message: 'Parsing failed.' } });
  result.update(model);
  expect(result.node.querySelector('.error').textContent).toBe('Parsing failed.');
  expect(result.node.querySelectorAll('tbody tr')).toHaveLength(3);
  const copies = [...result.node.querySelectorAll('button')].slice(0,3);
  expect(copies.map(button => button.disabled)).toEqual([true, true, false]);
  model.reset(); result.update(model);
  expect(result.node.querySelector('.conversion-buffer').hidden).toBe(true);
});

test('successful results render text safely and preserve buffer paging across unrelated updates', () => {
  const result = createHexparseResult(); const model = new ConversionOutputModel();
  model.setResult({ operation: 'parse', success: true, returnValue: 1, outputHex: '3C', outputText: '<script>', initialDestinationHex: '00 '.repeat(260), finalDestinationHex: '00 '.repeat(260) });
  result.update(model);
  expect(result.node.querySelector('script')).toBeNull();
  expect(result.node.querySelector('[aria-label="Parsed text (UTF-8)"]').textContent).toContain('<script>');
  result.node.querySelector('.conversion-buffer-navigation button:last-child').click();
  result.update(model);
  expect(result.node.querySelector('tbody td').textContent).toBe('128');
});

test('default service enables all four conversion tools', async () => {
  const fetchImpl = jest.fn(async () => ({ ok: true, json: async () => ({}) }));
  const service = createConversionService({ fetchImpl });
  expect(service.isAvailable('hexparse')).toBe(true);
  expect(service.isAvailable('bcd')).toBe(true);
  expect(service.isAvailable('bits')).toBe(true);
  expect(service.isAvailable('base64')).toBe(true);
  await service.convert('hexparse', {});
  expect(fetchImpl).toHaveBeenCalledTimes(1);
});
