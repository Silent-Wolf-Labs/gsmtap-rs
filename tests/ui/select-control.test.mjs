import { jest } from '@jest/globals';
import { createSelectControl } from '../../static/components/select-control.js';

test('creates a labeled dropdown with configured options', () => {
  const control = createSelectControl({
    id: 'example', label: 'Example', options: [['all', 'All'], ['one', 'One']], onChange: jest.fn(),
  });
  expect(control.node.tagName).toBe('LABEL');
  expect(control.node.textContent).toContain('Example');
  expect(control.node.querySelector('#example').options).toHaveLength(2);
  expect(control.value()).toBe('all');
  control.setHidden(true);
  expect(control.node.hidden).toBe(true);
});
