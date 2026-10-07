export function createConversionField({ documentRef, tool, definition, prefix = '', onChange }) {
  const key = `${prefix}${definition.key}`;
  const label = documentRef.createElement('label');
  const text = documentRef.createElement('span');
  text.textContent = definition.label;
  const input = documentRef.createElement(definition.type === 'select' ? 'select' : definition.type === 'textarea' ? 'textarea' : 'input');
  input.name = key;
  input.id = `conversion-${tool}-${key.replace('.', '-')}`;
  label.htmlFor = input.id;
  if (definition.type === 'select') {
    for (const [value, title] of definition.options) {
      const option = documentRef.createElement('option');
      option.value = value; option.textContent = title; input.append(option);
    }
  } else if (definition.type !== 'textarea') {
    input.type = definition.type;
    if (definition.type === 'number') {
      input.min = '0'; input.max = String(definition.maximum ?? 65536); input.step = '1';
    }
  }
  const error = documentRef.createElement('span');
  error.className = 'error'; error.id = `${input.id}-error`;
  input.setAttribute('aria-describedby', error.id);
  input.addEventListener(definition.type === 'select' || definition.type === 'checkbox' ? 'change' : 'input', () => {
    onChange(key, definition.type === 'checkbox' ? input.checked : input.value);
  });
  label.append(text, input, error);
  return { key, label, update(value, message, visible = true) {
    label.hidden = !visible;
    if (definition.type === 'checkbox') input.checked = Boolean(value);
    else if (documentRef.activeElement !== input) input.value = String(value);
    error.textContent = message || '';
    input.setAttribute('aria-invalid', String(Boolean(message)));
  } };
}
