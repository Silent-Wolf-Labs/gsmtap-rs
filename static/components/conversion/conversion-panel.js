import { createConversionField } from './conversion-field.js';
import { createHexparseResult } from './hexparse-result.js';
import { createBcdResult } from './bcd-result.js';
import { createBase64Result } from './base64-result.js';
import { createBitsResult } from './bits-result.js';
import { destinationFields } from '../../models/conversion/input-model.js';

export function createConversionPanels({ documentRef = document, controller }) {
  const views = new Map();
  const host = documentRef.querySelector('#conversion-panels');
  if (!host) return views;
  for (const tool of ['hexparse', 'bcd', 'bits', 'base64']) {
    const panel = host.querySelector(`[data-workbench-view="${tool}"]`);
    if (!panel) continue;
    const form = documentRef.createElement('form');
    form.noValidate = true;
    form.className = 'conversion-form';
    const inputFields = documentRef.createElement('fieldset');
    const inputLegend = documentRef.createElement('legend'); inputLegend.textContent = 'Input'; inputFields.append(inputLegend);
    const bufferFields = documentRef.createElement('fieldset');
    const bufferLegend = documentRef.createElement('legend'); bufferLegend.textContent = 'Destination buffer'; bufferFields.append(bufferLegend);
    const controls = new Map();

    function field(definition, parent, prefix = '') {
      const control = createConversionField({ documentRef, tool, definition, prefix,
        onChange: (key, value) => controller.update(tool, key, value) });
      parent.append(control.label);
      controls.set(control.key, control);
    }
    const state = controller.getState(tool);
    for (const definition of state.input.fields) field(definition, inputFields);
    for (const definition of destinationFields) field(definition, bufferFields, 'destination.');
    const actions = documentRef.createElement('div'); actions.className = 'conversion-actions';
    const convert = documentRef.createElement('button'); convert.type = 'submit'; convert.textContent = 'Convert';
    const reset = documentRef.createElement('button'); reset.type = 'button'; reset.textContent = 'Reset'; reset.addEventListener('click', () => controller.reset(tool));
    actions.append(convert, reset);
    const feedback = documentRef.createElement('p'); feedback.setAttribute('role', 'status'); feedback.setAttribute('aria-live', 'polite');
    const resultView = tool === 'hexparse' ? createHexparseResult({ documentRef })
      : tool === 'bcd' ? createBcdResult({ documentRef })
      : tool === 'bits' ? createBitsResult({ documentRef }) : createBase64Result({ documentRef });
    const result = resultView?.node || documentRef.createElement('pre');
    result.className = 'conversion-result'; result.setAttribute('aria-label', 'Conversion result');
    form.append(inputFields, bufferFields, actions, feedback, result);
    form.addEventListener('submit', event => { event.preventDefault(); void controller.submit(tool); });
    panel.append(form);
    const view = { update(current) {
      const visible = new Set(current.input.visibleFields.map(definition => definition.key));
      bufferFields.hidden = !current.input.usesDestination;
      for (const [key, control] of controls) {
        const buffer = key.startsWith('destination.');
        const value = buffer ? current.input.destination.values[key.slice(12)] : current.input.values[key];
        control.update(value, current.errors[key], buffer || visible.has(key));
      }
      convert.disabled = current.pending || !controller.isAvailable(tool);
      convert.textContent = current.pending ? 'Converting…' : 'Convert';
      feedback.className = current.requestError || Object.keys(current.errors).length ? 'error' : '';
      feedback.textContent = current.requestError || (Object.keys(current.errors).length ? 'Correct the highlighted input fields.' : !controller.isAvailable(tool) ? 'Conversion tools are not available yet.' : current.pending ? 'Converting…' : '');
      if (resultView) resultView.update(current.output);
      else result.textContent = current.output.hasResult ? JSON.stringify(current.output.result, null, 2) : 'No conversion result.';
    } };
    views.set(tool, view);
    view.update(state);
  }
  return views;
}
