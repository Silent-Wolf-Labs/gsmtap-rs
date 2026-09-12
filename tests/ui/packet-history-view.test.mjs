import { jest } from '@jest/globals';
import { createPacketHistoryView } from '../../static/components/packet/packet-history-view.js';

function setup() {
  document.body.innerHTML = `
    <button id="clear-filters" type="button" hidden>Clear filters</button>
    <button id="forward-selected" type="button" hidden>Forward selected (0)</button>
    <button id="clear-history" type="button" hidden>Clear history</button>
    <div id="packets"></div>
  `;
}

function modePanel() {
  return {
    setMode: jest.fn(),
    render: jest.fn((packets, selectedPacketId, options) => {
      document.querySelector('#packets').innerHTML = `
        <div class="packet-column-filter" data-filter-field="sourceAddress">
          <input class="packet-column-search" value="${options.filters.sourceAddress || ''}">
        </div>
        <p>${options.hasPackets ? 1 : 0}:${selectedPacketId || ''}</p>
      `;
    }),
  };
}

test('renders packet actions through the mode panel boundary', () => {
  setup();
  const panel = modePanel();
  const callbacks = {
    onClearFilters: jest.fn(),
    onForward: jest.fn(),
    onClearHistory: jest.fn(),
  };
  const view = createPacketHistoryView({ documentRef: document, modePanel: panel });

  view.render({
    packets: [{ id: 1 }],
    selectedPacketId: 1,
    mode: 'relay',
    filters: { sourceAddress: '192.0.2.1' },
    hasPackets: true,
    openFilter: null,
    selectedPacketIds: new Set([1]),
    forwardInFlight: false,
    clearHistoryInFlight: false,
    onSelectionChange: jest.fn(),
    onSelectAllChange: jest.fn(),
    onFilterOpenChange: jest.fn(),
    onFilterChange: jest.fn(),
    ...callbacks,
  });

  expect(panel.setMode).toHaveBeenCalledWith('relay');
  expect(panel.render).toHaveBeenCalledWith(
    [{ id: 1 }],
    1,
    expect.objectContaining({ filters: { sourceAddress: '192.0.2.1' } }),
  );
  expect(document.querySelector('#clear-filters').hidden).toBe(false);
  expect(document.querySelector('#forward-selected').hidden).toBe(false);
  expect(document.querySelector('#forward-selected').disabled).toBe(false);
  expect(document.querySelector('#forward-selected').textContent).toBe('Forward selected (1)');
  expect(document.querySelector('#clear-history').disabled).toBe(false);
});

test('wires packet actions and restores focused text filters', () => {
  setup();
  const panel = modePanel();
  const callbacks = {
    onClearFilters: jest.fn(),
    onForward: jest.fn(),
    onClearHistory: jest.fn(),
  };
  const view = createPacketHistoryView({ documentRef: document, modePanel: panel });
  const firstRender = {
    packets: [{ id: 1 }],
    selectedPacketId: null,
    mode: 'relay',
    filters: { sourceAddress: '' },
    hasPackets: true,
    openFilter: null,
    selectedPacketIds: new Set([1]),
    forwardInFlight: false,
    clearHistoryInFlight: false,
    onSelectionChange: jest.fn(),
    onSelectAllChange: jest.fn(),
    onFilterOpenChange: jest.fn(),
    onFilterChange: jest.fn(),
    ...callbacks,
  };
  view.render(firstRender);
  const input = document.querySelector('.packet-column-search');
  input.focus();
  input.value = '192.0.2.';
  input.setSelectionRange(2, 2);

  view.render({ ...firstRender, filters: { sourceAddress: input.value } });

  expect(document.activeElement).toBe(document.querySelector('.packet-column-search'));
  expect(document.activeElement.selectionStart).toBe(input.value.length);
  document.querySelector('#clear-filters').click();
  document.querySelector('#forward-selected').click();
  document.querySelector('#clear-history').click();
  expect(callbacks.onClearFilters).toHaveBeenCalledTimes(1);
  expect(callbacks.onForward).toHaveBeenCalledTimes(1);
  expect(callbacks.onClearHistory).toHaveBeenCalledTimes(1);
});

test('renders packet history load errors', () => {
  setup();
  const view = createPacketHistoryView({ documentRef: document, modePanel: modePanel() });

  view.renderError(new Error('offline'));

  expect(document.querySelector('#packets .error').textContent)
    .toBe('Unable to load packet history: offline');
});

test('handles missing DOM nodes and empty document gracefully', () => {
  const emptyDoc = {
    querySelector: () => null,
    createElement: () => ({ append: () => {}, classList: { contains: () => false } }),
    activeElement: null,
  };
  const view = createPacketHistoryView({ documentRef: emptyDoc, modePanel: modePanel() });
  expect(() => {
    view.render({
      packets: [],
      selectedPacketId: null,
      mode: 'listen',
      filters: {},
      hasPackets: false,
      openFilter: null,
      selectedPacketIds: new Set(),
      forwardInFlight: false,
      clearHistoryInFlight: false,
      onSelectionChange: () => {},
      onSelectAllChange: () => {},
      onFilterOpenChange: () => {},
      onFilterChange: () => {},
      onClearFilters: () => {},
      onForward: () => {},
      onClearHistory: () => {},
    });
    view.renderError(new Error('test'));
  }).not.toThrow();
});