import { applyTooltip } from '../tooltip.js';

export function createCard({ title, className = '' }) {
  const card = document.createElement('section');
  card.className = `card ${className}`.trim();
  const heading = document.createElement('h2');
  heading.textContent = title;
  card.append(heading);
  return card;
}

export function addCardItem(parent, label, value, className = '', tooltip = '') {
  const item = document.createElement('div');
  item.className = `card-item ${className}`.trim();
  applyTooltip(item, label, tooltip);
  const name = document.createElement('span');
  name.className = 'card-item-label';
  name.textContent = label;
  const content = document.createElement('strong');
  content.textContent = value;
  item.append(name, content);
  parent.append(item);
}
