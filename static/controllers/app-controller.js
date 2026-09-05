import {
  getStatus,
  getPackets,
  previewModification,
  sendModification,
} from '../services/api.js';
import { subscribeToUpdates } from '../services/events.js';
import { createModifyPanel } from '../components/panels/modify-panel.js';
import { createModePanel } from '../components/panels/mode-panel.js';

async function defaultRefreshWorkbench() {
  const [status, packets] = await Promise.all([getStatus(), getPackets()]);
  return { status, packets };
}

export function createAppController(documentRef = document, dependencies = {}) {
  const {
    previewModification: previewModificationRequest = previewModification,
    refreshWorkbench: refreshWorkbenchRequest = defaultRefreshWorkbench,
    sendModification: sendModificationRequest = sendModification,
    subscribeToUpdates: subscribeToUpdatesRequest = subscribeToUpdates,
    setInterval: setIntervalRequest = setInterval,
    clearInterval: clearIntervalRequest = clearInterval,
  } = dependencies;
  const packetsNode = documentRef.querySelector('#packets');
  const sendSectionNode = documentRef.querySelector('#send-section');
  const resultNode = documentRef.querySelector('#result');
  let activeMode = 'listen';
  let packets = [];
  let selectedPacketId = null;
  let interval;
  let unsubscribe;

  function showResult(text) {
    resultNode.textContent = text;
  }

  const modifyPanel = createModifyPanel({
    previewModification: previewModificationRequest,
    sendModification: sendModificationRequest,
    showResult,
  });
  const modePanel = createModePanel({
    documentRef,
    onModify: modifyPanel.selectPacket,
    onSelectPacket: packet => { selectedPacketId = packet?.id ?? null; },
  });

  function render() {
    modePanel.render(packets, selectedPacketId);
  }

  function applyStatus(status) {
    activeMode = status.mode;
    modePanel.applyStatus(status);
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
