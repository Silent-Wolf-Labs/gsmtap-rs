export function createSelectControl({ id, label, options, onChange }) {
  const labelNode = document.createElement('label');
  labelNode.textContent = label;
  const select = document.createElement('select');
  select.id = id;
  for (const [value, text] of options) {
    const option = document.createElement('option');
    option.value = value;
    option.textContent = text;
    select.append(option);
  }
  select.addEventListener('change', onChange);
  labelNode.append(select);

  return {
    node: labelNode,
    value: () => select.value,
    setHidden: hidden => { labelNode.hidden = hidden; },
  };
}
