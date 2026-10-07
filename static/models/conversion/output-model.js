export class ConversionOutputModel {
  constructor() { this.reset(); }
  reset() { this.result = null; }
  setResult(result) {
    if (!result || typeof result.operation !== 'string' || typeof result.success !== 'boolean')
      throw new Error('Invalid conversion response.');
    // Responses are JSON; copy them so later caller mutations cannot change
    // the retained result, including nested buffer and error information.
    this.result = JSON.parse(JSON.stringify(result));
  }
  get hasResult() { return this.result !== null; }
  get success() { return this.result?.success ?? null; }
  get returnValue() { return this.result?.returnValue ?? null; }
  get outputLength() { return this.result?.outputLength ?? null; }
  get error() { return this.result?.error ?? null; }
  get initialDestinationHex() { return this.result?.initialDestinationHex ?? null; }
  get finalDestinationHex() { return this.result?.finalDestinationHex ?? null; }
}
