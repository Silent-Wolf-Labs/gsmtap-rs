import { jest } from '@jest/globals';
import { createPacketHistoryController } from '../../static/controllers/packet-history-controller.js';

function setup() {
  document.body.innerHTML = `
    <button id="clear-history" type="button">Clear history</button>
    <dialog id="clear-history-dialog">
      <form method="dialog">
        <input id="clear-history-skip" type="checkbox">
        <button id="cancel-clear-history" type="submit">Cancel</button>
        <button id="confirm-clear-history" type="submit">Clear history</button>
      </form>
    </dialog>
  `;
  localStorage.clear();
}

function createView() {
  return {
    render: jest.fn(),
    renderError: jest.fn(),
  };
}

function lastState(view) {
  return view.render.mock.calls[view.render.mock.calls.length - 1][0];
}

function packets() {
  return [
    { id: 1, direction: 'RX', sourceAddress: '192.0.2.1:4729', decoded: {} },
    { id: 2, direction: 'RX', sourceAddress: '198.51.100.2:4729', decoded: {} },
  ];
}

beforeEach(setup);

test('updates packet history and prunes evicted Relay selections', () => {
  const view = createView();
  const controller = createPacketHistoryController({ documentRef: document, view });

  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onFilterChange({ field: 'packet', value: '2' });

  expect(lastState(view).packets).toEqual([packets()[1]]);
  expect(lastState(view).selectedPacketIds).toEqual(new Set([1]));

  controller.updatePackets([packets()[1]]);

  expect(lastState(view).selectedPacketIds).toEqual(new Set());
});

test('selects only visible packets and preserves hidden selections', () => {
  const view = createView();
  const controller = createPacketHistoryController({ documentRef: document, view });

  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onFilterChange({ field: 'packet', value: '2' });
  lastState(view).onSelectAllChange([2], true);

  expect(lastState(view).selectedPacketIds).toEqual(new Set([1, 2]));
  lastState(view).onSelectAllChange([2], false);
  expect(lastState(view).selectedPacketIds).toEqual(new Set([1]));
});

test('resets open filters and Modify-only fields when changing mode', () => {
  const view = createView();
  const controller = createPacketHistoryController({ documentRef: document, view });

  controller.setMode('modify');
  lastState(view).onFilterOpenChange('direction');
  lastState(view).onFilterChange({ field: 'direction', value: 'TX' });
  controller.setMode('listen');

  expect(lastState(view).mode).toBe('listen');
  expect(lastState(view).openFilter).toBe(null);
  expect(lastState(view).filters.direction).toBe(null);
});

test('forwards selected Relay packets and retains failures', async () => {
  const view = createView();
  const showResult = jest.fn();
  const requestRefresh = jest.fn(async () => {});
  const forwardPackets = jest.fn(async () => ({
    ok: true,
    json: async () => ({
      results: [
        { packetId: 1, status: 'sent' },
        { packetId: 2, status: 'error: unavailable' },
      ],
    }),
  }));
  const controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectAllChange([1, 2], true);

  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));

  expect(forwardPackets).toHaveBeenCalledWith([1, 2]);
  expect(requestRefresh).toHaveBeenCalledTimes(1);
  expect(lastState(view).selectedPacketIds).toEqual(new Set([2]));
  expect(showResult).toHaveBeenCalledWith('Forwarded 1 packet(s), 1 failed.');
  expect(lastState(view).forwardInFlight).toBe(false);
});

test('removes not-found packets and reports API failures', async () => {
  const view = createView();
  const showResult = jest.fn();
  const controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets: jest.fn(async () => ({
      ok: false,
      status: 404,
      text: async () => 'missing',
    })),
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets([{ id: 1 }]);
  lastState(view).onSelectionChange(1, true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));

  expect(lastState(view).selectedPacketIds).toEqual(new Set([1]));
  expect(showResult).toHaveBeenCalledWith('Forwarding failed (404): missing');
});

test('handles forwarding with all succeeded, not found, all failed, and exception states', async () => {
  const view = createView();
  const showResult = jest.fn();
  const requestRefresh = jest.fn(async () => {});

  // All succeeded
  let forwardPackets = jest.fn(async () => ({
    ok: true,
    json: async () => ({ results: [{ packetId: 1, status: 'sent' }] }),
  }));
  let controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(showResult).toHaveBeenCalledWith('Successfully forwarded 1 packet(s).');

  // All failed
  forwardPackets = jest.fn(async () => ({
    ok: true,
    json: async () => ({ results: [{ packetId: 1, status: 'error: failed' }] }),
  }));
  controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(showResult).toHaveBeenCalledWith('Forwarding failed for 1 packet(s).');

  // Not found only
  forwardPackets = jest.fn(async () => ({
    ok: true,
    json: async () => ({ results: [{ packetId: 1, status: 'not found' }] }),
  }));
  controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(showResult).toHaveBeenCalledWith('1 packet(s) not found.');

  // Not found and sent
  forwardPackets = jest.fn(async () => ({
    ok: true,
    json: async () => ({
      results: [
        { packetId: 1, status: 'sent' },
        { packetId: 2, status: 'not found' },
      ],
    }),
  }));
  controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectAllChange([1, 2], true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(showResult).toHaveBeenCalledWith('Forwarded 1 packet(s), 1 not found.');

  // Network/exception error
  forwardPackets = jest.fn(async () => { throw new Error('network down'); });
  controller = createPacketHistoryController({
    documentRef: document,
    view,
    forwardPackets,
    requestRefresh,
    showResult,
  });
  controller.setMode('relay');
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onForward();
  await new Promise(resolve => setTimeout(resolve, 0));
  expect(showResult).toHaveBeenCalledWith('Unable to forward packets: network down');

  // selectPacket and clearFilters
  controller.selectPacket({ id: 1 });
  controller.render();
  expect(lastState(view).selectedPacketId).toBe(1);
  controller.selectPacket(null);
  controller.render();
  expect(lastState(view).selectedPacketId).toBe(null);

  lastState(view).onFilterChange({ field: 'packet', value: '1' });
  lastState(view).onClearFilters();
  expect(lastState(view).filters.packet).toBe('');
});

test('clears history and resets controller state on success', async () => {
  localStorage.setItem('gsmtap.clear-history.skip-confirmation', 'true');
  const view = createView();
  const clearPacketHistory = jest.fn(async () => {});
  const controller = createPacketHistoryController({
    documentRef: document,
    view,
    clearPacketHistory,
  });
  controller.updatePackets(packets());
  lastState(view).onSelectionChange(1, true);
  lastState(view).onFilterChange({ field: 'packet', value: '1' });
  lastState(view).onClearHistory();
  await new Promise(resolve => setTimeout(resolve, 0));

  expect(clearPacketHistory).toHaveBeenCalledTimes(1);
  expect(lastState(view).packets).toEqual([]);
  expect(lastState(view).selectedPacketIds).toEqual(new Set());
  expect(lastState(view).filters.packet).toBe('');
});

test('reports clear-history failures and restores the action state', async () => {
  localStorage.setItem('gsmtap.clear-history.skip-confirmation', 'true');
  const view = createView();
  const showResult = jest.fn();
  const controller = createPacketHistoryController({
    documentRef: document,
    view,
    clearPacketHistory: jest.fn(async () => { throw new Error('server unavailable'); }),
    showResult,
  });
  controller.updatePackets([{ id: 1 }]);
  lastState(view).onClearHistory();
  await new Promise(resolve => setTimeout(resolve, 0));

  expect(showResult).toHaveBeenCalledWith('Unable to clear packet history: server unavailable');
  expect(lastState(view).clearHistoryInFlight).toBe(false);
  expect(lastState(view).packets).toEqual([{ id: 1 }]);
});