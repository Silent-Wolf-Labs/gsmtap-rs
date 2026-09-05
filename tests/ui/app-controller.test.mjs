import { jest } from '@jest/globals';
import { createAppController } from '../../static/controllers/app-controller.js';

function setup() {
  document.body.innerHTML = `
    <div id="status"></div>
    <section id="send-section" hidden>
      <p id="selected-packet"></p>
      <form id="send-form">
        <div id="fields"></div>
      </form>
      <section id="preview-card" hidden>
        <pre id="preview"></pre>
        <button id="confirm-send" type="button"></button>
      </section>
      <pre id="result"></pre>
    </section>
    <div id="filters"></div>
    <div id="packets"></div>
  `;
}

const status = mode => ({
  mode,
  gsmtapListen: '127.0.0.1:4729',
  gsmtapForward: null,
  stats: { received: 1, parseFailed: 0, ingressDropped: 0, historyDropped: 0, uiEventsDropped: 0 },
});

function dependencies(overrides = {}) {
  return {
    refreshWorkbench: jest.fn(async () => ({ status: status('listen'), packets: [] })),
    previewModification: jest.fn(),
    sendModification: jest.fn(),
    replayPacket: jest.fn(),
    subscribeToUpdates: jest.fn(() => jest.fn()),
    setInterval: jest.fn(() => 'interval'),
    clearInterval: jest.fn(),
    ...overrides,
  };
}

test('refreshes data and updates the mode-aware UI', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({ status: status('modify'), packets: [{ id: 1 }] })),
  });
  const controller = createAppController(document, api);

  await controller.refresh();

  expect(api.refreshWorkbench).toHaveBeenCalledTimes(1);
  expect(document.querySelector('.mode-badge').textContent).toBe('MODIFY');
  expect(document.querySelector('#send-section').hidden).toBe(false);
  expect(document.querySelectorAll('#packets tbody tr')).toHaveLength(1);
});

test('keeps listen packet inspection passive', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('listen'),
      packets: [{ id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 } }],
    })),
  });
  const controller = createAppController(document, api);

  await controller.refresh();
  document.querySelector('#packets tbody tr td').click();

  expect(document.querySelector('#packets pre').textContent).toContain('42');
  expect(document.querySelectorAll('#packets button')).toHaveLength(0);
});

test('retains selected packet details after refresh', async () => {
  setup();
  const packet = { id: 2, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 42 } };
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({ status: status('listen'), packets: [packet] })),
  });
  const controller = createAppController(document, api);

  await controller.refresh();
  document.querySelector('#packets tbody tr td').click();
  await controller.refresh();

  expect(document.querySelector('#packets .selected-row').textContent).toContain('#2');
  expect(document.querySelector('#packets .packet-details-row pre').textContent).toContain('42');
});

test('shows relay forwarding state without adding unsupported controls', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('relay'),
      packets: [{ id: 3, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: {}, forwardStatus: 'error: target unavailable' }],
    })),
  });
  const controller = createAppController(document, api);

  await controller.refresh();

  expect(document.querySelector('#packets th:nth-child(6)').textContent).toBe('Forward');
  expect(document.querySelector('#packets tbody tr').textContent).toContain('error: target unavailable');
  expect(document.querySelectorAll('#packets button')).toHaveLength(0);
});

test('renders a packet error when refresh fails', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => { throw new Error('server unavailable'); }),
  });
  const controller = createAppController(document, api);

  await controller.refresh();

  expect(document.querySelector('#packets .error').textContent)
    .toBe('Unable to load packet history: server unavailable');
});

test('starts polling and event updates, then cleans both up', () => {
  setup();
  const unsubscribe = jest.fn();
  const api = dependencies({ subscribeToUpdates: jest.fn(() => unsubscribe) });
  const controller = createAppController(document, api);

  controller.start();
  controller.start();
  controller.stop();

  expect(api.setInterval).toHaveBeenCalledTimes(1);
  expect(api.subscribeToUpdates).toHaveBeenCalledWith(expect.any(Function));
  expect(api.clearInterval).toHaveBeenCalledWith('interval');
  expect(unsubscribe).toHaveBeenCalledTimes(1);
});

test('uses the default refresh implementation and replays a packet', async () => {
  setup();
  const responses = {
    '/api/status': status('listen'),
    '/api/packets?limit=500': [],
  };
  global.fetch = jest.fn(async path => ({ json: async () => responses[path], text: async () => 'replayed' }));
  const controller = createAppController(document, { subscribeToUpdates: jest.fn(() => jest.fn()) });

  await controller.refresh();
  expect(fetch).toHaveBeenCalledWith('/api/status', expect.any(Object));
  expect(fetch).toHaveBeenCalledWith('/api/packets?limit=500', expect.any(Object));

  const replayable = { id: 15, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA', decoded: { arfcn: 1 } };
  const replayController = createAppController(document, {
    refreshWorkbench: jest.fn(async () => ({ status: status('modify'), packets: [replayable] })),
    replayPacket: jest.fn(async () => ({ text: async () => 'replayed' })),
  });
  await replayController.refresh();
  document.querySelector('#packets tbody tr').click();
  const replayButton = [...document.querySelectorAll('#packets button')].find(button => button.textContent === 'Replay');
  replayButton.click();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(document.querySelector('#result').textContent).toBe('replayed');
});

test('stops safely before start and on repeated cleanup', () => {
  setup();
  const api = dependencies();
  const controller = createAppController(document, api);

  controller.stop();
  controller.stop();
  expect(api.clearInterval).not.toHaveBeenCalled();
});
