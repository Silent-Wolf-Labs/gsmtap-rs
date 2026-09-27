function makeOption(documentRef, name, value, label, checked, onChange) {
  const wrapper = documentRef.createElement('label');
  wrapper.className = 'packet-column-filter-option';
  const input = documentRef.createElement('input');
  input.type = 'radio';
  input.name = name;
  input.value = value;
  input.checked = checked;
  input.addEventListener('change', () => onChange(value));
  wrapper.append(input, documentRef.createTextNode(label));
  return wrapper;
}

export function renderSelectFilter({ documentRef = document, field, value, options, onChange }) {
  const menu = documentRef.createDocumentFragment();
  menu.append(makeOption(documentRef, `filter-${field}`, '', 'All', !value, onChange));
  for (const [optionValue, optionLabel] of options) {
    menu.append(makeOption(documentRef, `filter-${field}`, optionValue, optionLabel, value === optionValue, onChange));
  }
  return menu;
}
