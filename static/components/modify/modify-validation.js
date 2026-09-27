import { hexFieldNames, numericFieldRanges, packetFieldNames } from './modify-fields.js';

export function validateModifyValues(values) {
  for (const name of packetFieldNames) {
    const value = Number(values[name]);
    const [minimum, maximum] = numericFieldRanges[name];
    if (!Number.isInteger(value) || value < minimum || value > maximum)
      return `${name} must be a whole number between ${minimum} and ${maximum}.`;
  }
  for (const name of hexFieldNames) {
    const value = (values[name] ?? '').replace(/\s/g, '');
    if (!/^[0-9a-f]*$/i.test(value) || value.length % 2 !== 0)
      return `${name} must contain complete hexadecimal bytes.`;
  }
  const extensionBytes = (values.extensionHex ?? '').replace(/\s/g, '').length / 2;
  if (extensionBytes % 4 !== 0) return 'extensionHex must contain a whole number of 4-byte header words.';
  const expectedHeaderWords = (16 + extensionBytes) / 4;
  if (values.headerLengthWords !== '' && Number(values.headerLengthWords) !== expectedHeaderWords)
    return `headerLengthWords must be ${expectedHeaderWords} for the extension length provided.`;
  return '';
}

export function readModifyForm(form) {
  return Object.fromEntries(new FormData(form));
}

export function validateModifyForm(form) {
  return validateModifyValues(readModifyForm(form));
}

export function toModifyPayload(values) {
  const payload = { ...values };
  for (const name of packetFieldNames) payload[name] = Number(payload[name]);
  return payload;
}
