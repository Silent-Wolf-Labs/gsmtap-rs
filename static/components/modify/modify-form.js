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

function countCompleteHexBytes(value) {
  return Math.floor((value ?? '').replace(/\s/g, '').length / 2);
}

function createByteField(documentRef, fields, name, labelText, onFieldChange, countNodes) {
  const group = documentRef.createElement('fieldset');
  group.className = `modify-byte-group modify-byte-group-${name}`;
  const legend = documentRef.createElement('legend');
  legend.textContent = `${labelText} · `;
  const count = documentRef.createElement('span');
  count.className = 'modify-byte-count';
  legend.append(count);
  applyTooltip(legend, name, fieldTooltips[name]);
  group.append(legend);

  const input = documentRef.createElement('textarea');
  input.name = name;
  input.className = `modify-byte-input modify-byte-input-${name}`;
  input.required = name === 'payloadHex';
  input.setAttribute('aria-label', labelText);
  if (name === 'extensionHex') input.pattern = '[0-9a-fA-F\\s]*';
  applyTooltip(input, name, fieldTooltips[name]);

  const updateCount = () => {
    count.textContent = `${countCompleteHexBytes(input.value)} bytes`;
    onFieldChange?.(name, input.value);
  };
  input.addEventListener('input', updateCount);
  group.append(input);
  fields.append(group);
  countNodes[name] = count;
  updateCount();
}

export function createModifyForm({ documentRef = document, form = documentRef.querySelector('#send-form'), onSubmit, onFieldChange }) {
  const fields = form.querySelector('#fields');
  const countNodes = {};
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
      input.addEventListener('input', () => onFieldChange?.(name, input.value));
      group.append(createLabel(documentRef, name, input));
    }
    fields.append(group);
  }
  const byteFields = documentRef.createElement('div');
  byteFields.className = 'modify-byte-fields';
  fields.append(byteFields);
  for (const [name, label] of [['extensionHex', 'Header extension (hex)'], ['payloadHex', 'Payload (hex)']])
    createByteField(documentRef, byteFields, name, label, onFieldChange, countNodes);
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
    setValues(values) {
      for (const name of Object.keys(values)) {
        if (!form.elements[name]) continue;
        form.elements[name].value = values[name] ?? '';
        if (countNodes[name]) countNodes[name].textContent = `${countCompleteHexBytes(form.elements[name].value)} bytes`;
      }
    },
    readValues,
    setPreviewEnabled(enabled) { previewButton.disabled = !enabled; },
    reset() {
      form.reset();
      for (const [name, count] of Object.entries(countNodes)) count.textContent = `${countCompleteHexBytes(form.elements[name].value)} bytes`;
      previewButton.disabled = true;
    },
  };
}
