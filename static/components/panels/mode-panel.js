import { createFilters } from '../filters.js';
import { renderPacketTable } from '../packet/packet-table.js';
import { renderStatusCard } from './cards/status-card.js';
import { listenCapability } from './listen-panel.js';
import { modifyCapability } from './modify-panel.js';
import { relayCapability, relayForwardingState } from './relay-panel.js';

export { relayForwardingState };

export const modeCapabilities = Object.freeze({ listen: listenCapability, relay: relayCapability, modify: modifyCapability });

export function createModePanel({
  documentRef = document,
  packetsNode = documentRef.querySelector('#packets'),
  filtersNode = documentRef.querySelector('#filters'),
  statusNode = documentRef.querySelector('#status'),
  onReplay,
  onModify,
  renderTable = renderPacketTable,
  renderStatus = renderStatusCard,
} = {}) {
  let capability = listenCapability;
  let activeMode = capability.mode;
  const filters = createFilters(filtersNode, render);

  function setMode(mode) {
    activeMode = mode;
    capability = modeCapabilities[mode] || listenCapability;
    filters.setMode(mode);
  }

  function applyStatus(status) {
    setMode(status.mode);
    renderStatus(statusNode, status);
  }

  function render(packets) {
    const actions = capability.actions;
    renderTable(packetsNode, filters.filter(packets), activeMode,
      capability.replay ? (actions.replay || onReplay) : undefined,
      capability.modify ? (actions.modify || onModify) : undefined);
  }

  return {
    get capability() { return capability; },
    filters,
    setMode,
    applyStatus,
    render,
  };
}
