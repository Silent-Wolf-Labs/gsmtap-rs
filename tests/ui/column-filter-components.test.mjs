import { jest } from '@jest/globals';
import { renderSelectFilter } from '../../static/components/packet/select-filter.js';
import { renderTextFilter } from '../../static/components/packet/text-filter.js';

test('renders categorical options with the current value selected', () => {
  const onChange = jest.fn();
  const menu = renderSelectFilter({
    field: 'direction',
    value: 'RX',
    options: [['RX', 'RX'], ['TX', 'TX']],
    onChange,
  });
  const node = document.createElement('div');
  node.append(menu);

  expect([...node.querySelectorAll('input')].map(input => input.value)).toEqual(['', 'RX', 'TX']);
  expect(node.querySelector('input[value="RX"]').checked).toBe(true);
  expect(node.querySelector('input[value=""]').checked).toBe(false);

  const tx = node.querySelector('input[value="TX"]');
  tx.checked = true;
  tx.dispatchEvent(new Event('change', { bubbles: true }));
  expect(onChange).toHaveBeenCalledWith('TX');
});

test('renders All selected when a categorical filter is empty', () => {
  const menu = renderSelectFilter({
    field: 'decode',
    value: null,
    options: [['success', 'Decoded'], ['error', 'Failed']],
    onChange: jest.fn(),
  });
  const node = document.createElement('div');
  node.append(menu);

  expect(node.querySelector('input[value=""]').checked).toBe(true);
  expect([...node.querySelectorAll('label')].map(label => label.textContent)).toEqual(['All', 'Decoded', 'Failed']);
});

test('renders an accessible text filter and emits input values', () => {
  const onChange = jest.fn();
  const input = renderTextFilter({
    label: 'Source address',
    value: '192.168',
    onChange,
  });

  expect(input.type).toBe('search');
  expect(input.value).toBe('192.168');
  expect(input.getAttribute('aria-label')).toBe('Search Source address');

  input.value = ':4729';
  input.dispatchEvent(new Event('input', { bubbles: true }));
  expect(onChange).toHaveBeenCalledWith(':4729');
});
