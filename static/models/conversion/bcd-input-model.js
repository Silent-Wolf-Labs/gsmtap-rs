import { ConversionInputModel, sourceFields } from './input-model.js';

export class BcdInputModel extends ConversionInputModel {
  constructor() {
    super({ operation: 'str2bcd', source: '', sourceEncoding: 'text', startNibble: 0, endNibble: 0, automaticEnd: true, allowHex: false }, [
      { key: 'operation', label: 'Operation', type: 'select', options: [['str2bcd', 'Digit string to BCD'], ['bcd2str', 'BCD to digit string'], ['char2bcd', 'Character to nibble'], ['bcd2char', 'Nibble to character']] },
      ...sourceFields,
      { key: 'startNibble', label: 'Start nibble (inclusive)', type: 'number', maximum: 131072, operations: ['str2bcd', 'bcd2str'] },
      { key: 'automaticEnd', label: 'Determine end from digit string', type: 'checkbox', operations: ['str2bcd'] },
      { key: 'endNibble', label: 'End nibble (exclusive)', type: 'number', maximum: 131072, operations: ['str2bcd', 'bcd2str'] },
      { key: 'allowHex', label: 'Allow hexadecimal digits A–F', type: 'checkbox', operations: ['str2bcd', 'bcd2str'] },
    ]);
  }
  get usesDestination() { return ['str2bcd', 'bcd2str'].includes(this.values.operation); }
  get visibleFields() {
    return super.visibleFields.filter(field => !(field.key === 'endNibble' && this.values.operation === 'str2bcd' && this.values.automaticEnd));
  }
  validate() {
    const errors = super.validate();
    if (['char2bcd', 'bcd2char'].includes(this.values.operation)) {
      const source = String(this.values.source);
      const compact = source.replace(/[ \t\r\n]/g, '');
      if (this.values.sourceEncoding === 'hex') {
        if (!errors.source && compact.length !== 2) errors.source = 'Supply exactly one source byte.';
        if (this.values.operation === 'bcd2char' && !errors.source && parseInt(compact, 16) > 15)
          errors.source = 'The BCD nibble must be between 00 and 0F.';
      } else if (!/^[\x00-\x7f]$/.test(source)) errors.source = 'Supply exactly one ASCII character, or use hex bytes.';
      else if (this.values.operation === 'bcd2char' && source.charCodeAt(0) > 15)
        errors.source = 'Use hex bytes to supply a nibble between 00 and 0F.';
    }
    return errors;
  }
  toRequest() {
    const request = super.toRequest();
    if (request.operation === 'str2bcd') {
      request.options.endNibble = this.values.automaticEnd ? null : Number(this.values.endNibble);
      delete request.options.automaticEnd;
    }
    return request;
  }
}
