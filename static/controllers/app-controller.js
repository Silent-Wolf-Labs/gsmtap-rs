import {
  previewModification,
  refreshWorkbench,
  replayPacket as apiReplayPacket,
  sendModification,
  subscribeToUpdates,
} from '../services/api.js';
import { renderStatusCard } from '../components/cards/status-card.js';
import { createFilters } from '../components/filters.js';
import { renderPacketTable } from '../components/packet-table.js';
import { createModifyPanel } from '../components/modify-panel.js';

export function createAppController(documentRef = document, dependencies = {}) {
  const {
    previewModification: previewModificationRequest = previewModification,
    refreshWorkbench: refreshWorkbenchRequest = refreshWorkbench,
    replayPacket: replayPacketRequest = apiReplayPacket,
    sendModification: sendModificationRequest = sendModification,
    subscribeToUpdates: subscribeToUpdatesRequest = subscribeToUpdates,
    setInterval: setIntervalRequest = setInterval,
    clearInterval: clearIntervalRequest = clearInterval,
  } = dependencies;
  const packetsNode = documentRef.querySelector('#packets');
  const statusNode = documentRef.querySelector('#status');
  const filtersNode = documentRef.querySelector('#filters');
  const sendSectionNode = documentRef.querySelector('#send-section');
  const resultNode = documentRef.querySelector('#result');
  let activeMode = 'listen';
  let packets = [];
  let interval;
  let unsubscribe;

  function showResult(text) {
    resultNode.textContent = text;
  }

  const filters = createFilters(filtersNode, render);
  const modifyPanel = createModifyPanel({
    previewModification: previewModificationRequest,
    sendModification: sendModificationRequest,
    showResult,
  });

  function render() {
    renderPacketTable(packetsNode, filters.filter(packets), activeMode, replayPacket, modifyPanel.selectPacket);
  }

  async function replayPacket(id) {
    const response = await replayPacketRequest(id);
    showResult(await response.text());
  }

  function applyStatus(status) {
    activeMode = status.mode;
    renderStatusCard(statusNode, status);
    filters.setMode(activeMode);
    sendSectionNode.hidden = activeMode !== 'modify';
  }

  async function refresh() {
    try {
      const data = await refreshWorkbenchRequest();
      applyStatus(data.status);
      packets = data.packets;
      render();
    } catch (error) {
      packetsNode.replaceChildren();
      const message = documentRef.createElement('p');
      message.className = 'error';
      message.textContent = `Unable to load packet history: ${error.message}`;
      packetsNode.append(message);
    }
  }

  function start() {
    if (interval) return;
    refresh();
    interval = setIntervalRequest(refresh, 1000);
    unsubscribe = subscribeToUpdatesRequest(refresh);
  }

  function stop() {
    if (interval) clearIntervalRequest(interval);
    if (unsubscribe) unsubscribe();
    interval = undefined;
    unsubscribe = undefined;
  }

  return { start, stop, refresh };
}
