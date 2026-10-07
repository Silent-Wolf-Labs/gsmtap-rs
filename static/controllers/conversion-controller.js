import { HexparseInputModel } from '../models/conversion/hexparse-input-model.js';
import { BcdInputModel } from '../models/conversion/bcd-input-model.js';
import { BitsInputModel } from '../models/conversion/bits-input-model.js';
import { Base64InputModel } from '../models/conversion/base64-input-model.js';
import { ConversionOutputModel } from '../models/conversion/output-model.js';
import { createConversionService } from '../services/conversion-service.js';

const INPUT_MODELS = { hexparse: HexparseInputModel, bcd: BcdInputModel, bits: BitsInputModel, base64: Base64InputModel };

export function createConversionController({ service = createConversionService(), onChange = () => {} } = {}) {
  const states = Object.fromEntries(Object.entries(INPUT_MODELS).map(([tool, Model]) => [tool, {
    input: new Model(), output: new ConversionOutputModel(), pending: false, errors: {}, requestError: '', revision: 0, abort: null,
  }]));
  function stateFor(tool) {
    if (!Object.hasOwn(states, tool)) throw new Error(`Unknown conversion tool: ${tool}`);
    return states[tool];
  }
  function notify(tool) { onChange(tool, stateFor(tool)); }
  const isAvailable = tool => service.isAvailable ? service.isAvailable(tool) : service.available;
  function invalidate(state) {
    state.revision += 1;
    state.abort?.abort();
    state.abort = null;
    state.pending = false;
    state.errors = {};
    state.requestError = '';
    state.output.reset();
  }
  return {
    get available() { return service.available; },
    isAvailable,
    getState: stateFor,
    update(tool, key, value) {
      const state = stateFor(tool);
      if (key.startsWith('destination.')) state.input.destination.update(key.slice(12), value);
      else state.input.update(key, value);
      invalidate(state);
      notify(tool);
    },
    reset(tool) {
      const state = stateFor(tool);
      invalidate(state);
      state.input.reset();
      notify(tool);
    },
    async submit(tool) {
      const state = stateFor(tool);
      if (state.pending) return;
      state.errors = state.input.validate();
      state.requestError = '';
      if (Object.keys(state.errors).length) { notify(tool); return; }
      if (!isAvailable(tool)) { state.requestError = 'Conversion tools are not available yet.'; notify(tool); return; }
      state.output.reset();
      state.pending = true;
      const revision = ++state.revision;
      const abort = new AbortController();
      state.abort = abort;
      notify(tool);
      try {
        const result = await service.convert(tool, state.input.toRequest(), { signal: abort.signal });
        if (state.revision === revision) state.output.setResult(result);
      } catch (error) {
        if (state.revision === revision) state.requestError = error.message || 'Conversion request failed.';
      } finally {
        if (state.revision === revision) {
          state.pending = false;
          state.abort = null;
          notify(tool);
        }
      }
    },
    stop() {
      for (const [tool, state] of Object.entries(states)) {
        state.revision += 1;
        state.abort?.abort();
        state.abort = null;
        state.pending = false;
        notify(tool);
      }
    },
  };
}
