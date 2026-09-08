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
    <button id="clear-filters" type="button" hidden>Clear filters</button>
    <button id="forward-selected" type="button" hidden>Forward selected (0)</button>
    <button id="clear-history" type="button" hidden>Clear history</button>
    <dialog id="clear-history-dialog">
      <form method="dialog">
        <input id="clear-history-skip" type="checkbox">
        <button id="cancel-clear-history" type="submit">Cancel</button>
        <button id="confirm-clear-history" type="submit">Clear history</button>
      </form>
    </dialog>
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
    clearPacketHistory: jest.fn(async () => {}),
    setCapturePaused: jest.fn(async paused => ({ capturePaused: paused })),
    previewModification: jest.fn(),
    sendModification: jest.fn(),
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
  expect(document.querySelector('.mode-control').textContent).toBe('Modify▾');
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

  expect(document.querySelector('#packets .gsmtap-header-table').textContent).toContain('42');
  expect(document.querySelectorAll('#packets .packet-column-filter-trigger')).toHaveLength(4);
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
  document.querySelector('#packets .packet-original-input').open = true;
  await controller.refresh();

  expect(document.querySelector('#packets .selected-row').textContent).toContain('#2');
  expect(document.querySelector('#packets .packet-details-row .gsmtap-header-table').textContent).toContain('42');
  expect(document.querySelector('#packets .packet-original-input').open).toBe(true);
});

test('coalesces refreshes and applies capture state immediately', async () => {
  setup();
  const releases = [];
  const refreshWorkbench = jest.fn(() => new Promise(resolve => releases.push(resolve)));
  const setCapturePaused = jest.fn(async paused => ({ capturePaused: paused }));
  const api = dependencies({ refreshWorkbench, setCapturePaused });
  const controller = createAppController(document, api);

  const firstRefresh = controller.refresh();
  const secondRefresh = controller.refresh();
  expect(refreshWorkbench).toHaveBeenCalledTimes(1);

  releases.shift()({ status: { ...status('listen'), capturePaused: false }, packets: [] });
  await firstRefresh;
  await secondRefresh;
  expect(refreshWorkbench).toHaveBeenCalledTimes(2);
  const queuedRefresh = controller.refresh();
  releases.shift()({ status: { ...status('listen'), capturePaused: false }, packets: [] });
  await queuedRefresh;
  const button = document.querySelector('.capture-toggle');
  button.click();
  await Promise.resolve();
  await Promise.resolve();

  expect(setCapturePaused).toHaveBeenCalledWith(true);
  expect(document.querySelector('.capture-toggle').textContent).toBe('Resume Capture');
});

test('does not let an older refresh overwrite a completed capture change', async () => {
  setup();
  const refreshResolvers = [];
  const refreshWorkbench = jest.fn(() => new Promise(resolve => refreshResolvers.push(resolve)));
  const setCapturePaused = jest.fn(async paused => ({ capturePaused: paused }));
  const api = dependencies({ refreshWorkbench, setCapturePaused });
  const controller = createAppController(document, api);

  const initialRefresh = controller.refresh();
  refreshResolvers.shift()({ status: { ...status('listen'), capturePaused: false }, packets: [] });
  await initialRefresh;

  const oldRefresh = controller.refresh();
  const button = document.querySelector('.capture-toggle');
  const captureChange = button.click();
  await Promise.resolve();

  expect(setCapturePaused).toHaveBeenCalledWith(true);
  expect(document.querySelector('.capture-toggle').textContent).toBe('Resume Capture');

  refreshResolvers.shift()({ status: { ...status('listen'), capturePaused: false }, packets: [] });
  await oldRefresh;
  expect(document.querySelector('.capture-toggle').textContent).toBe('Resume Capture');

  await Promise.resolve();
  expect(refreshWorkbench).toHaveBeenCalledTimes(3);
  refreshResolvers.shift()({ status: { ...status('listen'), capturePaused: true }, packets: [] });
  await captureChange;
  expect(document.querySelector('.capture-toggle').textContent).toBe('Resume Capture');
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

  expect(document.querySelector('#packets th:nth-child(7)').textContent).toBe('Forward');
  expect(document.querySelector('#packets tbody tr').textContent).toContain('error: target unavailable');
  expect(document.querySelectorAll('#packets .packet-column-filter-trigger')).toHaveLength(4);
});

test('filters packet history through header controls and clears filters', async () => {
  setup();
  const packets = [
    { id: 1, direction: 'RX', timestampMs: Date.now(), sourceAddress: '192.0.2.1:4729', decoded: {} },
    { id: 2, direction: 'RX', timestampMs: Date.now(), sourceAddress: '198.51.100.2:4729', parseError: 'invalid' },
  ];
  const api = dependencies({ refreshWorkbench: jest.fn(async () => ({ status: status('listen'), packets })) });
  const controller = createAppController(document, api);
  await controller.refresh();

  document.querySelector('[aria-label="Filter Decode"]').click();
  document.querySelector('input[value="error"]').click();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(1);
  expect(document.querySelector('#clear-filters').hidden).toBe(false);

  document.querySelector('#clear-filters').click();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(2);
  expect(document.querySelector('#clear-filters').hidden).toBe(true);
});

test('clears packet history through the history action', async () => {
  setup();
  const clearPacketHistory = jest.fn(async () => {});
  const api = dependencies({
    clearPacketHistory,
    refreshWorkbench: jest.fn(async () => ({
      status: status('listen'),
      packets: [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  expect(document.querySelector('#clear-history').hidden).toBe(false);
  document.querySelector('#clear-history').click();
  document.querySelector('#confirm-clear-history').click();
  await Promise.resolve();
  expect(clearPacketHistory).toHaveBeenCalledTimes(1);
  expect(document.querySelector('#clear-history').hidden).toBe(false);
  expect(document.querySelector('#clear-history').disabled).toBe(true);
  expect(document.querySelector('#packets').textContent).toContain('No packets captured');
});

test('requires confirmation before clearing history and remembers the opt-out', async () => {
  setup();
  localStorage.removeItem('gsmtap.clear-history.skip-confirmation');
  const clearPacketHistory = jest.fn(async () => {});
  const api = dependencies({
    clearPacketHistory,
    refreshWorkbench: jest.fn(async () => ({
      status: status('listen'),
      packets: [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  document.querySelector('#clear-history').click();
  expect(clearPacketHistory).not.toHaveBeenCalled();
  document.querySelector('#clear-history-skip').checked = true;
  document.querySelector('#confirm-clear-history').click();
  await Promise.resolve();
  expect(clearPacketHistory).toHaveBeenCalledTimes(1);
  expect(localStorage.getItem('gsmtap.clear-history.skip-confirmation')).toBe('true');
});

test('keeps Clear history available in every workbench mode', async () => {
  setup();
  const modes = ['listen', 'relay', 'modify'];
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status(modes.shift()),
      packets: [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);

  for (let index = 0; index < 3; index += 1) {
    await controller.refresh();
    expect(document.querySelector('#clear-history').hidden).toBe(false);
    expect(document.querySelector('#clear-history').disabled).toBe(false);
  }
});

test('keeps an open column filter menu open across packet refreshes', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('listen'),
      packets: [{ id: 1, direction: 'RX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();
  document.querySelector('[aria-label="Filter Decode"]').click();
  expect(document.querySelector('[aria-label="Filter Decode"]')
    .closest('.packet-column-filter').querySelector('.packet-column-filter-menu').hidden).toBe(false);

  await controller.refresh();
  expect(document.querySelector('[aria-label="Filter Decode"]')
    .closest('.packet-column-filter').querySelector('.packet-column-filter-menu').hidden).toBe(false);
});

test('keeps the text-filter input focused while filtering', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('listen'),
      packets: [{ id: 1, direction: 'RX', timestampMs: Date.now(), sourceAddress: '192.0.2.1:4729', decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();
  document.querySelector('[aria-label="Filter Source address"]').click();
  const search = document.querySelector('[aria-label="Search Source address"]');
  search.value = '192.0.2';
  search.dispatchEvent(new Event('input', { bubbles: true }));

  const replacement = document.querySelector('[aria-label="Search Source address"]');
  expect(replacement.value).toBe('192.0.2');
  expect(document.activeElement).toBe(replacement);
});

test('closes a categorical filter menu after a selection', async () => {
  setup();
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('modify'),
      packets: [{ id: 1, direction: 'TX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();
  document.querySelector('[aria-label="Filter Direction"]').click();
  document.querySelector('input[value="TX"]').click();
  const trigger = [...document.querySelectorAll('.packet-column-filter-trigger')]
    .find(candidate => candidate.getAttribute('aria-label')?.startsWith('Filter Direction'));
  expect(trigger.getAttribute('aria-expanded')).toBe('false');
});

test('resets Modify-only filters when returning to Listen mode', async () => {
  setup();
  const responses = [
    { status: status('modify'), packets: [{ id: 1, direction: 'TX', timestampMs: Date.now(), decoded: {}, modified: true }] },
    { status: status('listen'), packets: [{ id: 2, direction: 'RX', timestampMs: Date.now(), decoded: {} }] },
  ];
  const api = dependencies({ refreshWorkbench: jest.fn(async () => responses.shift()) });
  const controller = createAppController(document, api);
  await controller.refresh();
  document.querySelector('[aria-label="Filter Direction"]').click();
  document.querySelector('input[value="TX"]').click();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(1);

  await controller.refresh();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(1);
  expect(document.querySelector('#packets tbody > tr').textContent).toContain('#2');
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

test('uses the default refresh implementation', async () => {
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
});

test('stops safely before start and on repeated cleanup', () => {
  setup();
  const api = dependencies();
  const controller = createAppController(document, api);

  controller.stop();
  controller.stop();
  expect(api.clearInterval).not.toHaveBeenCalled();
});

test('tracks Relay selection, updates Forward button, and prunes missing packets on refresh', async () => {
  setup();
  let packetList = [
    { id: 10, direction: 'RX', timestampMs: Date.now(), decoded: {} },
    { id: 11, direction: 'RX', timestampMs: Date.now(), decoded: {} },
  ];
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({
      status: status('relay'),
      packets: packetList,
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  const forwardBtn = document.querySelector('#forward-selected');
  expect(forwardBtn.hidden).toBe(false);
  expect(forwardBtn.disabled).toBe(true);
  expect(forwardBtn.textContent).toBe('Forward selected (0)');

  // Select packet 10
  document.querySelector('input[aria-label="Select packet 10"]').click();
  expect(forwardBtn.disabled).toBe(false);
  expect(forwardBtn.textContent).toBe('Forward selected (1)');

  // Select packet 11
  document.querySelector('input[aria-label="Select packet 11"]').click();
  expect(forwardBtn.textContent).toBe('Forward selected (2)');

  // Unselect packet 10
  document.querySelector('input[aria-label="Select packet 10"]').click();
  expect(forwardBtn.textContent).toBe('Forward selected (1)');

  // Packet 11 gets evicted on refresh
  packetList = [{ id: 10, direction: 'RX', timestampMs: Date.now(), decoded: {} }];
  await controller.refresh();

  // Packet 11 is pruned from selection
  expect(forwardBtn.disabled).toBe(true);
  expect(forwardBtn.textContent).toBe('Forward selected (0)');
});

test('orchestrates batch forwarding, locks in-flight requests, and handles partial outcomes', async () => {
  setup();
  let forwardResolver;
  const forwardPackets = jest.fn(() => new Promise(resolve => { forwardResolver = resolve; }));
  let packetList = [
    { id: 10, direction: 'RX', timestampMs: Date.now(), decoded: {} },
    { id: 11, direction: 'RX', timestampMs: Date.now(), decoded: {} },
  ];
  const api = dependencies({
    forwardPackets,
    refreshWorkbench: jest.fn(async () => ({
      status: status('relay'),
      packets: packetList,
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  // Select all visible packets via header checkbox
  document.querySelector('.packet-select-all').click();
  const forwardBtn = document.querySelector('#forward-selected');
  expect(forwardBtn.textContent).toBe('Forward selected (2)');

  // Click forward
  forwardBtn.click();
  expect(forwardPackets).toHaveBeenCalledWith([10, 11]);
  expect(forwardBtn.disabled).toBe(true);

  // Attempt duplicate click while in-flight
  forwardBtn.click();
  expect(forwardPackets).toHaveBeenCalledTimes(1);

  // Complete forward with partial failure: 10 sent, 11 failed
  forwardResolver({
    ok: true,
    json: async () => ({
      results: [
        { packetId: 10, status: 'sent' },
        { packetId: 11, status: 'error: socket unreachable' },
      ],
    }),
  });
  await new Promise(resolve => setTimeout(resolve, 10));

  expect(document.querySelector('#result').textContent).toBe('Forwarded 1 packet(s), 1 failed.');
  // Packet 10 was sent and removed from selection; packet 11 failed and remains selected
  expect(forwardBtn.textContent).toBe('Forward selected (1)');
  expect(forwardBtn.disabled).toBe(false);
});

test('handles forwarding API failure and displays error', async () => {
  setup();
  const forwardPackets = jest.fn(async () => ({
    ok: false,
    status: 400,
    text: async () => 'duplicate packet id',
  }));
  const api = dependencies({
    forwardPackets,
    refreshWorkbench: jest.fn(async () => ({
      status: status('relay'),
      packets: [{ id: 10, direction: 'RX', timestampMs: Date.now(), decoded: {} }],
    })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  document.querySelector('input[aria-label="Select packet 10"]').click();
  document.querySelector('#forward-selected').click();
  await Promise.resolve();
  await Promise.resolve();

  expect(document.querySelector('#result').textContent).toContain('Forwarding failed (400): duplicate packet id');
  expect(document.querySelector('#forward-selected').textContent).toBe('Forward selected (1)');
});

test('select-all toggles only visible filtered packets while preserving hidden selections', async () => {
  setup();
  const packets = [
    { id: 10, direction: 'RX', timestampMs: Date.now(), sourceAddress: '192.0.2.1:4729', decoded: {} },
    { id: 11, direction: 'RX', timestampMs: Date.now(), sourceAddress: '198.51.100.2:4729', parseError: 'invalid' },
  ];
  const api = dependencies({
    refreshWorkbench: jest.fn(async () => ({ status: status('relay'), packets })),
  });
  const controller = createAppController(document, api);
  await controller.refresh();

  // Select packet 10
  document.querySelector('input[aria-label="Select packet 10"]').click();
  expect(document.querySelector('#forward-selected').textContent).toBe('Forward selected (1)');

  // Filter to only error decode (packet 11)
  document.querySelector('[aria-label="Filter Decode"]').click();
  document.querySelector('input[value="error"]').click();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(1);

  // In filtered view, packet 11 is unselected, so header checkbox is unchecked
  const headerSelect = document.querySelector('.packet-select-all');
  expect(headerSelect.checked).toBe(false);

  // Toggle select-all in filtered view (adds packet 11)
  headerSelect.click();
  expect(document.querySelector('#forward-selected').textContent).toBe('Forward selected (2)');

  // Clear filters
  document.querySelector('#clear-filters').click();
  expect(document.querySelectorAll('#packets tbody > tr')).toHaveLength(2);
  // Both packet 10 and 11 are selected now
  expect(document.querySelector('.packet-select-all').checked).toBe(true);
});
