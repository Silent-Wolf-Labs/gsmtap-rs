import { jest } from '@jest/globals';
import { createFilters, filterPackets, visibleFilterIds } from '../../static/components/filters.js';

const packets = [
  { direction: 'RX', parseError: undefined, modified: false },
  { direction: 'RX', parseError: 'truncated', modified: false },
  { direction: 'TX', parseError: undefined, modified: true },
];

test('exposes only applicable controls by mode', () => {
  expect(visibleFilterIds('listen')).toEqual(['parse']);
  expect(visibleFilterIds('relay')).toEqual(['parse']);
  expect(visibleFilterIds('modify')).toEqual(['direction', 'parse', 'modified']);
});

test('filters successful received packets', () => {
  expect(filterPackets(packets, { direction: 'RX', parse: 'success', modified: 'all' })).toHaveLength(1);
});

test('uses all as the default value for omitted filters', () => {
  expect(filterPackets(packets, {})).toEqual(packets);
});

test('filters parse failures and modified packets', () => {
  expect(filterPackets(packets, { parse: 'error' })).toHaveLength(1);
  expect(filterPackets(packets, { direction: 'TX', parse: 'success', modified: 'yes' })).toHaveLength(1);
});

test('creates and updates mode-aware filter controls in the DOM', () => {
  const node = document.createElement('div');
  const render = jest.fn();
  const filters = createFilters(node, render);
  expect(node.querySelectorAll('select')).toHaveLength(1);
  filters.setMode('relay');
  expect(node.querySelectorAll('select')).toHaveLength(1);
  filters.setMode('modify');
  filters.setMode('modify');
  expect(node.querySelector('#filter-direction')).not.toBeNull();
  expect(node.querySelector('#filter-modified')).not.toBeNull();
  node.querySelector('#filter-parse').dispatchEvent(new Event('change'));
  expect(render).toHaveBeenCalled();
});

test('filters through the mounted controls', () => {
  const node = document.createElement('div');
  const filters = createFilters(node, jest.fn());

  expect(filters.filter(packets)).toEqual(packets);
});
