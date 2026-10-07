import { readFileSync } from 'node:fs';
import { jest } from '@jest/globals';
import { createConversionController } from '../../static/controllers/conversion-controller.js';
import { createNavigationController } from '../../static/controllers/navigation-controller.js';
import { createConversionPanels } from '../../static/components/conversion/conversion-panel.js';
import { createAppController } from '../../static/controllers/app-controller.js';

function setup(service) {
  document.documentElement.innerHTML = readFileSync(new URL('../../static/index.html', import.meta.url), 'utf8');
  let views;
  const controller = createConversionController({ service, onChange: (tool, state) => views.get(tool).update(state) });
  views = createConversionPanels({ controller });
  const navigation = createNavigationController(); navigation.start();
  return { controller, views, navigation };
}
function select(view) {
  const selector = document.querySelector('#workbench-view'); selector.value = view; selector.dispatchEvent(new Event('change'));
}
function panel(tool) { return document.querySelector(`#conversion-panels [data-workbench-view="${tool}"]`); }

test('navigation shows one view and retains each tool input while switching', () => {
  const { navigation } = setup();
  expect(document.querySelector('[data-workbench-view="packets"]').hidden).toBe(false);
  select('hexparse');
  const input = panel('hexparse').querySelector('[name="source"]'); input.value = 'CA FE'; input.dispatchEvent(new Event('input'));
  select('bcd'); select('hexparse');
  expect(input.value).toBe('CA FE');
  expect([...document.querySelectorAll('[data-workbench-view]')].filter(node => !node.hidden)).toHaveLength(1);
  navigation.select('unknown'); expect(panel('hexparse').hidden).toBe(false);
  navigation.stop(); select('bcd'); expect(panel('hexparse').hidden).toBe(false);
  navigation.start(); expect(panel('bcd').hidden).toBe(false);
});

test('forms have labeled inputs, disabled conversion buttons, and functional reset', () => {
  const { controller } = setup({ available: false });
  for (const tool of ['hexparse', 'bcd', 'bits', 'base64']) {
    expect(panel(tool).querySelector('button[type="submit"]').disabled).toBe(true);
    expect(panel(tool).textContent).toContain('Conversion tools are not available yet.');
    for (const input of panel(tool).querySelectorAll('input, textarea, select')) {
      expect(document.querySelector(`label[for="${input.id}"]`)).not.toBeNull();
    }
  }
  controller.update('hexparse', 'source', 'AA');
  panel('hexparse').querySelector('button[type="button"]').click();
  expect(panel('hexparse').querySelector('[name="source"]').value).toBe('');
});

test('operation changes expose relevant fields without resetting source', () => {
  const { controller } = setup();
  const bcd = panel('bcd');
  expect(bcd.querySelector('[name="endNibble"]').closest('label').hidden).toBe(true);
  const automatic = bcd.querySelector('[name="automaticEnd"]'); automatic.checked = false; automatic.dispatchEvent(new Event('change'));
  expect(bcd.querySelector('[name="endNibble"]').closest('label').hidden).toBe(false);
  const operation = bcd.querySelector('[name="operation"]'); operation.value = 'char2bcd'; operation.dispatchEvent(new Event('change'));
  expect(bcd.querySelectorAll('fieldset')[1].hidden).toBe(true);
  const probe = panel('base64').querySelector('[name="sizeProbe"]');
  controller.update('base64', 'operation', 'decode'); probe.checked = true; probe.dispatchEvent(new Event('change'));
  expect(panel('base64').querySelectorAll('fieldset')[1].hidden).toBe(true);
});

test('validation errors and async results stay local to the submitted panel', async () => {
  const service = { available: true, convert: jest.fn(async () => ({ operation: 'pack', success: true, finalDestinationHex: '80' })) };
  const { controller } = setup(service);
  const bits = panel('bits'); controller.update('bits', 'source', 'GG');
  bits.querySelector('form').dispatchEvent(new Event('submit', { cancelable: true }));
  expect(bits.querySelector('[name="source"]').getAttribute('aria-invalid')).toBe('true');
  expect(bits.querySelector('[role="status"]').textContent).toContain('highlighted');
  controller.update('bits', 'source', '01');
  await controller.submit('bits');
  expect(bits.querySelector('.conversion-result').textContent).toContain('80');
  expect(panel('bcd').querySelector('.conversion-result').textContent).toBe('No conversion result.');
});

test('missing conversion shell is supported by existing workbench controller fixtures', () => {
  document.body.innerHTML = '';
  expect(createConversionPanels({ controller: createConversionController() }).size).toBe(0);
  const navigation = createNavigationController(); navigation.start(); navigation.stop();
});

test('packet refresh and mode changes preserve conversion view, form nodes, inputs, and results', async () => {
  document.documentElement.innerHTML = readFileSync(new URL('../../static/index.html', import.meta.url), 'utf8');
  let mode = 'listen';
  const app = createAppController(document, {
    refreshWorkbench: jest.fn(async () => ({ status: { mode, gsmtapListen: '127.0.0.1:4729', gsmtapForward: '127.0.0.1:9000' }, packets: [] })),
    subscribeToUpdates: jest.fn(() => jest.fn()),
    setInterval: jest.fn(() => 1), clearInterval: jest.fn(),
    conversionService: { available: true, convert: jest.fn(async () => ({ operation: 'parse', success: true, finalDestinationHex: 'CA' })) },
  });
  app.start(); await app.refresh(); select('hexparse');
  const input = panel('hexparse').querySelector('[name="source"]');
  input.value = 'CA'; input.dispatchEvent(new Event('input'));
  panel('hexparse').querySelector('form').dispatchEvent(new Event('submit', { cancelable: true }));
  await Promise.resolve(); await Promise.resolve();
  expect(panel('hexparse').querySelector('.conversion-result').textContent).toContain('CA');
  for (const next of ['relay', 'modify', 'listen']) {
    mode = next; await app.refresh();
    expect(panel('hexparse').hidden).toBe(false);
    expect(document.querySelector('[data-workbench-view="packets"]').hidden).toBe(true);
    expect(panel('hexparse').querySelector('[name="source"]')).toBe(input);
    expect(input.value).toBe('CA');
    expect(panel('hexparse').querySelector('.conversion-result').textContent).toContain('CA');
  }
  select('bcd'); select('hexparse');
  expect(panel('hexparse').querySelector('.conversion-result').textContent).toContain('CA');
  select('packets');
  expect(document.querySelector('[data-workbench-view="packets"]').hidden).toBe(false);
  app.stop();
});
