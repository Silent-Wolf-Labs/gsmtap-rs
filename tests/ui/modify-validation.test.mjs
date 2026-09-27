import { readModifyForm, toModifyPayload, validateModifyValues } from '../../static/components/modify/modify-validation.js';

const validValues = Object.fromEntries([
  ['version', '1'], ['headerLengthWords', '4'], ['messageType', '1'], ['timeslot', '1'], ['arfcn', '1'],
  ['signalDbm', '1'], ['snrDb', '1'], ['frameNumber', '1'], ['subtype', '1'], ['antennaNumber', '1'],
  ['subSlot', '1'], ['reserved', '1'], ['extensionHex', ''], ['payloadHex', 'CA'],
]);

test('validates plain values without a DOM form', () => {
  expect(validateModifyValues(validValues)).toBe('');
  expect(validateModifyValues({ ...validValues, arfcn: '65536' })).toContain('arfcn');
});

test('rejects missing, fractional, and negative numeric values', () => {
  expect(validateModifyValues({ ...validValues, headerLengthWords: '' })).toContain('headerLengthWords');
  expect(validateModifyValues({ ...validValues, signalDbm: '1.5' })).toContain('signalDbm');
  expect(validateModifyValues({ ...validValues, timeslot: '-1' })).toContain('timeslot');
});

test('validates hexadecimal byte boundaries and header length', () => {
  expect(validateModifyValues({ ...validValues, payloadHex: 'GG' })).toContain('payloadHex');
  expect(validateModifyValues({ ...validValues, payloadHex: 'A' })).toContain('payloadHex');
  expect(validateModifyValues({ ...validValues, extensionHex: 'CA' })).toContain('extensionHex');
  expect(validateModifyValues({ ...validValues, extensionHex: 'CA FE BA BE', headerLengthWords: '4' })).toContain('headerLengthWords');
  expect(validateModifyValues({ ...validValues, extensionHex: 'CA FE BA BE', headerLengthWords: '5' })).toBe('');
});

test('converts numeric fields only at the payload boundary', () => {
  expect(toModifyPayload(validValues)).toMatchObject({ version: 1, frameNumber: 1, payloadHex: 'CA' });
});

test('reads form controls into plain values', () => {
  document.body.innerHTML = '<form><input name="version" value="1"></form>';
  expect(readModifyForm(document.querySelector('form'))).toEqual({ version: '1' });
});
