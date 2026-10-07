import { BufferInputModel, MAX_BUFFER_BYTES, MAX_SOURCE_CHARACTERS } from '../../static/models/conversion/input-model.js';
import { HexparseInputModel } from '../../static/models/conversion/hexparse-input-model.js';
import { BcdInputModel } from '../../static/models/conversion/bcd-input-model.js';
import { BitsInputModel } from '../../static/models/conversion/bits-input-model.js';
import { Base64InputModel } from '../../static/models/conversion/base64-input-model.js';
import { ConversionOutputModel } from '../../static/models/conversion/output-model.js';

test.each([HexparseInputModel, BcdInputModel, BitsInputModel, Base64InputModel])('input model %p starts valid and resets without sharing state', Model => {
  const input = new Model();
  const other = new Model();
  expect(input.validate()).toEqual({});
  input.update('source', 'AA');
  input.destination.update('capacity', '2');
  expect(other.values.source).toBe('');
  expect(other.destination.values.capacity).toBe(64);
  input.reset();
  expect(input.values.source).toBe('');
  expect(input.destination.values.capacity).toBe(64);
  expect(() => input.update('unknown', 0)).toThrow('Unknown conversion field');
});

test('buffer configuration validates bounds and exact initialized storage', () => {
  const buffer = new BufferInputModel();
  buffer.update('capacity', '2'); buffer.update('fillByte', '170'); buffer.update('initialHex', 'AA 00');
  expect(buffer.validate()).toEqual({});
  expect(buffer.toRequest()).toEqual({ capacity: 2, fillByte: 170, initialHex: 'AA 00' });
  buffer.update('initialHex', 'AA');
  expect(buffer.validate().initialHex).toContain('exactly match');
  buffer.update('initialHex', 'AG');
  expect(buffer.validate().initialHex).toContain('hexadecimal');
  buffer.update('capacity', MAX_BUFFER_BYTES + 1); buffer.update('fillByte', -1);
  expect(buffer.validate()).toHaveProperty('capacity');
  expect(buffer.validate()).toHaveProperty('fillByte');
  expect(() => buffer.update('unknown', 0)).toThrow('Unknown buffer field');
});

test.each(['', '-1', '1.5', 'Infinity', 'NaN', '9007199254740992'])('rejects invalid capacity %s', value => {
  const model = new HexparseInputModel(); model.destination.update('capacity', value);
  expect(model.validate()).toHaveProperty(['destination.capacity']);
});

test('hexparse preserves malformed parser input and embedded NUL, but validates byte representation', () => {
  const model = new HexparseInputModel();
  model.update('source', 'A Z\0F');
  expect(model.validate()).toEqual({});
  expect(model.toRequest().source).toEqual({ encoding: 'text', data: 'A Z\0F' });
  model.update('sourceEncoding', 'hex');
  expect(model.validate()).toHaveProperty('source');
  model.update('source', '41 20 5A 00 46');
  expect(model.validate()).toEqual({});
  model.update('source', 'A'.repeat(MAX_SOURCE_CHARACTERS + 1));
  expect(model.validate()).toHaveProperty('source');
  model.update('operation', 'invalid');
  expect(model.validate()).toHaveProperty('operation');
});

test('BCD preserves implicit end and exposes explicit nibble ranges', () => {
  const model = new BcdInputModel();
  model.update('source', '123'); model.update('endNibble', 'invalid');
  expect(model.validate()).toEqual({});
  expect(model.toRequest().options).toEqual({ startNibble: 0, endNibble: null, allowHex: false });
  model.update('automaticEnd', false);
  expect(model.validate()).toHaveProperty('endNibble');
  model.update('endNibble', '3');
  expect(model.toRequest().options.endNibble).toBe(3);
  model.update('operation', 'bcd2str');
  expect(model.visibleFields.some(field => field.key === 'endNibble')).toBe(true);
  expect(model.toRequest().options).not.toHaveProperty('automaticEnd');
});

test('BCD scalar operations validate one byte and ignore destination configuration', () => {
  const model = new BcdInputModel(); model.update('operation', 'char2bcd');
  model.destination.update('capacity', -1);
  model.update('source', 'g'); // Invalid BCD digits must reach Rust to observe its contract.
  expect(model.validate()).toEqual({});
  expect(model.toRequest().destination).toBeNull();
  expect(model.toRequest().options).toEqual({});
  model.update('source', 'é');
  expect(model.validate()).toHaveProperty('source');
  model.update('sourceEncoding', 'hex'); model.update('source', 'FF');
  expect(model.validate()).toEqual({});
  model.update('operation', 'bcd2char');
  expect(model.validate()).toHaveProperty('source');
  model.update('source', '0F');
  expect(model.validate()).toEqual({});
  model.update('source', '0F 01');
  expect(model.validate()).toHaveProperty('source');
});

test('bits sends byte-sized source and extended offset options without implementing conversions', () => {
  const model = new BitsInputModel(); model.update('source', '00 01 FF'); model.update('numBits', '3');
  expect(model.validate()).toEqual({});
  expect(model.toRequest().options).toEqual({ numBits: 3 });
  model.update('operation', 'pack-ext'); model.update('inputOffset', '1'); model.update('outputOffset', '7'); model.update('lsbMode', true);
  expect(model.toRequest().options).toEqual({ numBits: 3, inputOffset: 1, outputOffset: 7, lsbMode: true });
  model.update('lsbMode', 'true');
  expect(model.validate()).toHaveProperty('lsbMode');
  model.update('operation', 'wrong');
  expect(model.validate()).toHaveProperty('operation');
});

test('Base64 decode size probe omits destination and preserves initial output-length value', () => {
  const model = new Base64InputModel(); model.update('operation', 'decode'); model.update('source', 'invalid base64'); model.update('initialOutputLength', '99');
  model.destination.update('capacity', -1); model.update('sizeProbe', true);
  expect(model.validate()).toEqual({});
  expect(model.toRequest()).toMatchObject({ destination: null, options: { sizeProbe: true, initialOutputLength: 99 } });
  model.update('operation', 'encode');
  expect(model.validate()).toHaveProperty(['destination.capacity']);
  expect(model.toRequest().options).not.toHaveProperty('sizeProbe');
});

test('output retains failure buffers and distinguishes returned value from output length', () => {
  const output = new ConversionOutputModel();
  expect(output.hasResult).toBe(false);
  expect(output.success).toBeNull();
  expect(output.returnValue).toBeNull();
  expect(output.outputLength).toBeNull();
  expect(output.error).toBeNull();
  expect(output.initialDestinationHex).toBeNull();
  expect(output.finalDestinationHex).toBeNull();
  const result = { operation: 'decode', success: false, returnValue: -22, outputLength: 99, initialDestinationHex: 'AA AA', finalDestinationHex: '4D AA', error: { code: 'invalidInput' } };
  output.setResult(result); result.error.code = 'changed';
  expect(output.success).toBe(false);
  expect(output.returnValue).toBe(-22);
  expect(output.outputLength).toBe(99);
  expect(output.error.code).toBe('invalidInput');
  expect(output.initialDestinationHex).toBe('AA AA');
  expect(output.finalDestinationHex).toBe('4D AA');
  expect(() => output.setResult({})).toThrow('Invalid conversion response');
  output.reset(); expect(output.hasResult).toBe(false);
});
