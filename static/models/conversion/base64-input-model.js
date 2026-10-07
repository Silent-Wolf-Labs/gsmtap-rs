import { ConversionInputModel, sourceFields } from './input-model.js';

export class Base64InputModel extends ConversionInputModel {
  constructor() {
    super({ operation: 'encode', source: '', sourceEncoding: 'text', sizeProbe: false, initialOutputLength: 0 }, [
      { key: 'operation', label: 'Operation', type: 'select', options: [['encode', 'Encode bytes to Base64'], ['decode', 'Decode Base64 to bytes']] },
      ...sourceFields,
      { key: 'sizeProbe', label: 'Probe decoded size without a destination', type: 'checkbox', operations: ['decode'] },
      { key: 'initialOutputLength', label: 'Initial output-length value', type: 'number', maximum: 4294967295 },
    ]);
  }
  get usesDestination() { return !(this.values.operation === 'decode' && this.values.sizeProbe); }
}
