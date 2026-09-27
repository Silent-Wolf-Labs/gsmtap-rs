export function renderTextFilter({ documentRef = document, label, value, onChange }) {
  const input = documentRef.createElement('input');
  input.type = 'search';
  input.className = 'packet-column-search';
  input.placeholder = 'Search';
  input.value = value ?? '';
  input.setAttribute('aria-label', `Search ${label}`);
  input.addEventListener('input', () => onChange(input.value));
  return input;
}
