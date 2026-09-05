import { jest } from '@jest/globals';
import { createModePanel, listenCapability, relayForwardingState } from '../../static/components/mode-panel.js';

function setup() {
  document.body.innerHTML = '<div id="status"></div><div id="filters"></div><div id="packets"></div>';
}

test('listen capability is passive and exposes packet inspection', () => {
  expect(listenCapability.inspect).toBe(true);
  expect(listenCapability.modify).toBe(false);
  expect(listenCapability.replay).toBe(false);
  expect(listenCapability.forwarding).toBe(false);
  expect(listenCapability.actions).toEqual({});
});

test('listen mode renders decoded packet details without actions', () => {
  setup();
  const panel = createModePanel();
  panel.render([{ id: 1, direction: 'RX', timestampMs: Date.now(), peer: 'peer', rawHex: 'CA FE', decoded: { arfcn: 42 } }]);
  expect(document.querySelector('#packets pre').textContent).toContain('42');
  expect(document.querySelectorAll('#packets button')).toHaveLength(0);
});

test('mode panel changes capabilities without inventing controls', () => {
  setup();
  const renderTable = jest.fn();
  const panel = createModePanel({ renderTable });
  panel.setMode('relay');
  panel.render([]);
  expect(panel.capability.mode).toBe('relay');
  expect(panel.capability.forwarding).toBe(true);
  expect(renderTable).toHaveBeenCalledWith(document.querySelector('#packets'), [], 'relay', undefined, undefined);
});

test('maps relay forwarding success, failure, and pending states', () => {
  expect(relayForwardingState({ forwardStatus: 'sent' })).toEqual({ status: 'sent', failed: false });
  expect(relayForwardingState({ forwardStatus: 'error: destination unreachable' })).toEqual({ status: 'error: destination unreachable', failed: true });
  expect(relayForwardingState({})).toEqual({ status: 'Pending', failed: false });
});
