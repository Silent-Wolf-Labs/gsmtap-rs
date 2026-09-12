import {
  getStatus,
  getPackets,
  clearPacketHistory,
  forwardPackets,
  previewModification,
  sendModification,
  setCapturePaused,
} from '../services/api.js';
import { subscribeToUpdates } from '../services/events.js';
import { createModifyPanel } from '../components/panels/modify-panel.js';
import { createModePanel } from '../components/panels/mode-panel.js';
import { createPacketHistoryView } from '../components/packet/packet-history-view.js';
import { createPacketHistoryController } from './packet-history-controller.js';

async function defaultRefreshWorkbench() {
  const [status, packets] = await Promise.all([getStatus(), getPackets()]);
  return { status, packets };
}

export function createAppController(documentRef = document, dependencies = {}) {
  const {
    forwardPackets: forwardPacketsRequest = forwardPackets,
    previewModification: previewModificationRequest = previewModification,
    refreshWorkbench: refreshWorkbenchRequest = defaultRefreshWorkbench,
    clearPacketHistory: clearPacketHistoryRequest = clearPacketHistory,
    sendModification: sendModificationRequest = sendModification,
    setCapturePaused: setCapturePausedRequest = setCapturePaused,
    subscribeToUpdates: subscribeToUpdatesRequest = subscribeToUpdates,
    setInterval: setIntervalRequest = setInterval,
    clearInterval: clearIntervalRequest = clearInterval,
  } = dependencies;
  const sendSectionNode = documentRef.querySelector('#send-section');
  const resultNode = documentRef.querySelector('#result');
  let activeMode = 'listen';
  let interval;
  let unsubscribe;
  let currentStatus;
  let refreshInFlight;
  let refreshQueued = false;
  let statusRevision = 0;

  function showResult(text) {
    resultNode.textContent = text;
  }

  const modifyPanel = createModifyPanel({
    previewModification: previewModificationRequest,
    sendModification: sendModificationRequest,
    showResult,
  });
  let packetHistory;
  const modePanel = createModePanel({
    documentRef,
    onModify: modifyPanel.selectPacket,
    onSelectPacket: packet => packetHistory.selectPacket(packet),
    onCaptureToggle: async paused => {
      const response = await setCapturePausedRequest(paused);
      statusRevision += 1;
      if (currentStatus) {
        applyStatus({
          ...currentStatus,
          capturePaused: response?.capturePaused ?? paused,
        });
      }
      await refresh();
    },
  });
  const packetHistoryView = createPacketHistoryView({ documentRef, modePanel });
  packetHistory = createPacketHistoryController({
    documentRef,
    view: packetHistoryView,
    forwardPackets: forwardPacketsRequest,
    clearPacketHistory: clearPacketHistoryRequest,
    requestRefresh: refresh,
    showResult,
  });

  function applyStatus(status) {
    currentStatus = status;
    activeMode = status.mode;
    modePanel.applyStatus(status);
    packetHistory.setMode(status.mode);
    sendSectionNode.hidden = activeMode !== 'modify';
  }

  async function performRefresh() {
    const requestedStatusRevision = statusRevision;
    try {
      const data = await refreshWorkbenchRequest();
      if (requestedStatusRevision === statusRevision) applyStatus(data.status);
      packetHistory.updatePackets(data.packets);
    } catch (error) {
      packetHistory.renderError(error);
    }
  }

  function refresh() {
    if (refreshInFlight) {
      refreshQueued = true;
      return refreshInFlight;
    }
    refreshInFlight = performRefresh().finally(() => {
      refreshInFlight = undefined;
      if (refreshQueued) {
        refreshQueued = false;
        void refresh();
      }
    });
    return refreshInFlight;
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
