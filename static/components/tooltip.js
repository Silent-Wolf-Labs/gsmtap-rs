export function applyTooltip(element, label, description) {
  if (!description) return element;
  element.title = description;
  element.setAttribute('aria-label', `${label}: ${description}`);
  return element;
}