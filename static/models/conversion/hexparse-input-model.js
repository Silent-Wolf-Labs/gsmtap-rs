import { ConversionInputModel, sourceFields } from './input-model.js';

export class HexparseInputModel extends ConversionInputModel {
  constructor() {
    super({ operation: 'parse', source: '', sourceEncoding: 'text' }, sourceFields);
  }
  validate() {
    const errors = super.validate();
    if (this.values.operation !== 'parse') errors.operation = 'Choose hexadecimal parsing.';
    // Text is the parser's input, including malformed hex and embedded NUL.
    // Hex format represents those source bytes, not the parsed destination.
    return errors;
  }
}
