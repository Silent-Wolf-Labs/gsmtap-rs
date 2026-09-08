import { renderSelectFilter } from './select-filter.js';
import { renderTextFilter } from './text-filter.js';

export function createColumnFilter({ documentRef = document, label, field, value, options, text, open = false, onOpenChange, onChange }) {
  const wrapper = documentRef.createElement('span');
  wrapper.className = 'packet-column-filter';
  wrapper.dataset.filterField = field;
  const trigger = documentRef.createElement('button');
  trigger.type = 'button';
  trigger.className = 'packet-column-filter-trigger';
  trigger.setAttribute('aria-label', `Filter ${label}`);
  trigger.setAttribute('aria-expanded', String(open));
  trigger.textContent = '⌄';

  const menu = documentRef.createElement('span');
  menu.className = 'packet-column-filter-menu';
  menu.hidden = !open;
  menu.classList.toggle('open', open);
  menu.setAttribute('role', 'group');
  const menuLabel = documentRef.createElement('span');
  menuLabel.className = 'packet-column-filter-menu-label';
  menuLabel.textContent = label;
  menu.append(menuLabel);

  const handleChange = nextValue => {
    if (!text) setOpen(false);
    onChange(nextValue);
  };

  if (text) {
    menu.append(renderTextFilter({ documentRef, label, value, onChange: handleChange }));
  } else {
    menu.append(renderSelectFilter({ documentRef, field, value, options, onChange: handleChange }));
  }

  function setOpen(open) {
    menu.hidden = !open;
    menu.classList.toggle('open', open);
    trigger.setAttribute('aria-expanded', String(open));
    onOpenChange?.(open);
    if (open) (menu.querySelector('input:not([type="radio"])') || menu.querySelector('input:checked'))?.focus();
  }

  trigger.addEventListener('click', event => {
    event.preventDefault();
    event.stopPropagation();
    setOpen(menu.hidden);
  });
  trigger.addEventListener('keydown', event => {
    if (event.key === 'Escape') {
      setOpen(false);
      trigger.focus();
    }
  });
  menu.addEventListener('change', () => { if (!text) setOpen(false); });
  menu.addEventListener('keydown', event => {
    if (event.key !== 'Escape') return;
    setOpen(false);
    trigger.focus();
  });
  wrapper.append(trigger, menu);
  return { node: wrapper, trigger, setOpen };
}
