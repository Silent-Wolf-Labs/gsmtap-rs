import { renderPacketTable } from '../packet/packet-table.js';
import { renderStatusCard } from '../cards/status-card.js';
import { listenCapability } from './listen-panel.js';
import { modifyCapability } from './modify-panel.js';
import { relayCapability, relayForwardingState } from './relay-panel.js';

export { listenCapability, relayCapability, relayForwardingState };

export const modeCapabilities = Object.freeze({ listen: listenCapability, relay: relayCapability, modify: modifyCapability });

export function createModePanel({
  documentRef = document,
  packetsNode = documentRef.querySelector('#packets'),
  statusNode = documentRef.querySelector('#status'),
  onModify,
  onSelectPacket,
  renderTable = renderPacketTable,
  renderStatus = renderStatusCard,
  onCaptureToggle,
  onModeChange,
  onModeError,
} = {}) {
  let capability = listenCapability;
  let activeMode = capability.mode;
  function setMode(mode) {
    activeMode = mode;
    capability = modeCapabilities[mode] || listenCapability;
  }

  function applyStatus(status) {
    setMode(status.mode);
    renderStatus(statusNode, status, { onCaptureToggle, onModeChange, onModeError });
  }

  function render(packets, selectedPacketId, tableOptions = {}) {
    const actions = capability.actions;
    renderTable(packetsNode, packets, activeMode,
      capability.modify ? (actions.modify || onModify) : undefined,
      { selectedPacketId, onSelectPacket, ...tableOptions });
  }

  return {
    get capability() { return capability; },
    setMode,
    applyStatus,
    render,
  };
}
