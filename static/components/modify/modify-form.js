import { fieldGroups, fieldTooltips, hexFieldNames, numericFieldRanges } from './modify-fields.js';
import { applyTooltip } from '../tooltip.js';

function createLabel(documentRef, name, input) {
  const label = documentRef.createElement('label');
  label.textContent = name;
  applyTooltip(label, name, fieldTooltips[name]);
  applyTooltip(input, name, fieldTooltips[name]);
  label.append(input);
  return label;
}

export function createModifyForm({ documentRef = document, form = documentRef.querySelector('#send-form'), onSubmit }) {
  const fields = form.querySelector('#fields');
  fields.replaceChildren();
  for (const [groupName, names] of fieldGroups) {
    const group = documentRef.createElement('fieldset');
    group.className = 'modify-field-group';
    const legend = documentRef.createElement('legend');
    legend.textContent = groupName;
    group.append(legend);
    for (const name of names) {
      const input = documentRef.createElement('input');
      input.name = name;
      input.type = 'number';
      input.min = numericFieldRanges[name][0];
      input.max = numericFieldRanges[name][1];
      input.step = '1';
      input.required = true;
      group.append(createLabel(documentRef, name, input));
    }
    fields.append(group);
  }
  for (const name of hexFieldNames) {
    const input = documentRef.createElement('textarea');
    input.name = name;
    input.required = name === 'payloadHex';
    if (name === 'extensionHex') input.pattern = '[0-9a-fA-F\\s]*';
    form.append(createLabel(documentRef, name, input));
  }
  const actions = documentRef.createElement('div');
  actions.className = 'modify-form-actions';
  const previewButton = documentRef.createElement('button');
  previewButton.type = 'submit';
  previewButton.disabled = true;
  previewButton.textContent = 'Preview changes';
  actions.append(previewButton);
  form.append(actions);
  form.addEventListener('submit', event => onSubmit(event, readValues()));

  function readValues() {
    return Object.fromEntries(new FormData(form));
  }
  return {
    setValues(values) { for (const name of Object.keys(values)) if (form.elements[name]) form.elements[name].value = values[name] ?? ''; },
    readValues,
    setPreviewEnabled(enabled) { previewButton.disabled = !enabled; },
  };
}
