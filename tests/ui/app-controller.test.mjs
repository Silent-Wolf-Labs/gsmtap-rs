import { jest } from '@jest/globals';
import { createAppController } from '../../static/controllers/app-controller.js';

function setup() {
  document.body.innerHTML = `
    <div id="status"></div>
    <section id="send-section" hidden>
      <p id="selected-packet"></p>
      <form id="send-form">
        <div id="fields"></div>
        <textarea name="extensionHex"></textarea>
        <textarea name="payloadHex"></textarea>
        <button id="confirm-send" type="button"></button>
      </form>
      <pre id="preview"></pre>
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
