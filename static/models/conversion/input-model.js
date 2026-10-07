export const MAX_BUFFER_BYTES = 65536;
export const MAX_SOURCE_CHARACTERS = MAX_BUFFER_BYTES * 3;

export function integerError(value, label, maximum = MAX_BUFFER_BYTES) {
  return value === '' || !Number.isSafeInteger(Number(value)) || Number(value) < 0 || Number(value) > maximum
    ? `${label} must be a whole number between 0 and ${maximum}.` : '';
}

export function hexError(value) {
  const compact = String(value).replace(/[ \t\r\n]/g, '');
  return /^[0-9a-f]*$/i.test(compact) && compact.length % 2 === 0
    ? '' : 'Enter complete hexadecimal bytes.';
}

export class BufferInputModel {
  constructor() { this.reset(); }
  reset() { this.values = { capacity: 64, fillByte: 0, initialHex: '' }; }
  update(key, value) {
    if (!Object.hasOwn(this.values, key)) throw new Error(`Unknown buffer field: ${key}`);
    this.values[key] = value;
  }
  validate() {
    const errors = {};
    for (const [key, label, maximum] of [['capacity', 'Destination capacity', MAX_BUFFER_BYTES], ['fillByte', 'Fill byte', 255]]) {
      const error = integerError(this.values[key], label, maximum);
      if (error) errors[key] = error;
    }
    const initial = this.values.initialHex;
    if (initial) {
      const error = hexError(initial);
      if (error) errors.initialHex = error;
      else if (initial.replace(/[ \t\r\n]/g, '').length / 2 !== Number(this.values.capacity))
        errors.initialHex = 'Initial bytes must exactly match destination capacity.';
    }
    return errors;
  }
  toRequest() {
    return { capacity: Number(this.values.capacity), fillByte: Number(this.values.fillByte), initialHex: this.values.initialHex };
  }
}

export class ConversionInputModel {
  constructor(defaults, fields) {
    this.defaults = defaults;
    this.fields = fields;
    this.destination = new BufferInputModel();
    this.reset();
  }
  reset() { this.values = { ...this.defaults }; this.destination.reset(); }
  update(key, value) {
    if (!Object.hasOwn(this.values, key)) throw new Error(`Unknown conversion field: ${key}`);
    this.values[key] = value;
  }
  get usesDestination() { return true; }
  get visibleFields() { return this.fields.filter(field => !field.operations || field.operations.includes(this.values.operation)); }
  validate() {
    const errors = {};
    for (const field of this.visibleFields) {
      const value = this.values[field.key];
      if (field.type === 'select' && !field.options.some(([option]) => option === value))
        errors[field.key] = `Choose a valid ${field.label.toLowerCase()}.`;
      if (field.type === 'number') {
        const error = integerError(value, field.label, field.maximum);
        if (error) errors[field.key] = error;
      }
      if (field.type === 'checkbox' && typeof value !== 'boolean') errors[field.key] = `${field.label} must be true or false.`;
    }
    if (String(this.values.source).length > MAX_SOURCE_CHARACTERS) errors.source = 'Source input is too large.';
    if (this.values.sourceEncoding === 'hex') {
      const error = hexError(this.values.source);
      if (error) errors.source = error;
    }
    if (this.usesDestination) {
      for (const [key, error] of Object.entries(this.destination.validate())) errors[`destination.${key}`] = error;
    }
    return errors;
  }
  toRequest() {
    const options = {};
    for (const field of this.visibleFields) {
      if (['operation', 'source', 'sourceEncoding'].includes(field.key)) continue;
      options[field.key] = field.type === 'number' ? Number(this.values[field.key]) : this.values[field.key];
    }
    return {
      operation: this.values.operation,
      source: { encoding: this.values.sourceEncoding, data: this.values.source },
      destination: this.usesDestination ? this.destination.toRequest() : null,
      options,
    };
  }
}

export const sourceFields = [
  { key: 'sourceEncoding', label: 'Source format', type: 'select', options: [['text', 'Text (UTF-8)'], ['hex', 'Hex bytes']] },
  { key: 'source', label: 'Source input', type: 'textarea' },
];

export const destinationFields = [
  { key: 'capacity', label: 'Destination capacity (bytes)', type: 'number', maximum: MAX_BUFFER_BYTES },
  { key: 'fillByte', label: 'Initial fill byte (decimal)', type: 'number', maximum: 255 },
  { key: 'initialHex', label: 'Initial destination bytes (hex, optional)', type: 'textarea' },
];
