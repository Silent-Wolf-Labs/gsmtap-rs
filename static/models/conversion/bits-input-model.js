import { ConversionInputModel, sourceFields } from './input-model.js';

export class BitsInputModel extends ConversionInputModel {
  constructor() {
    super({ operation: 'pack', source: '', sourceEncoding: 'hex', numBits: 0, inputOffset: 0, outputOffset: 0, lsbMode: false }, [
      { key: 'operation', label: 'Operation', type: 'select', options: [['pack', 'Pack byte-sized bits'], ['unpack', 'Unpack packed bytes'], ['pack-ext', 'Pack with offsets'], ['unpack-ext', 'Unpack with offsets']] },
      ...sourceFields.map(field => field.key === 'sourceEncoding'
        ? { ...field, options: [...field.options, ['bits', 'Unpacked bits (0/1)']] } : field),
      { key: 'numBits', label: 'Number of bits', type: 'number', maximum: 524288 },
      { key: 'inputOffset', label: 'Input offset', type: 'number', maximum: 524288, operations: ['pack-ext', 'unpack-ext'] },
      { key: 'outputOffset', label: 'Output offset', type: 'number', maximum: 524288, operations: ['pack-ext', 'unpack-ext'] },
      { key: 'lsbMode', label: 'Use LSB-first ordering', type: 'checkbox', operations: ['pack-ext', 'unpack-ext'] },
    ]);
  }
  validate() {
    const errors = super.validate();
    if (this.values.sourceEncoding === 'bits') {
      if (this.values.operation.startsWith('unpack')) errors.sourceEncoding = 'Unpacking requires packed source bytes.';
      if (!/^[01 \t\r\n]*$/.test(this.values.source)) errors.source = 'Enter only 0, 1, and ASCII whitespace.';
    }
    return errors;
  }
}
