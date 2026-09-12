import {
  getStatus,
  getPackets,
  clearPacketHistory,
  forwardPackets,
  previewModification,
  sendModification,
  setCapturePaused,
  setMode,
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
    setMode: setModeRequest = setMode,
    subscribeToUpdates: subscribeToUpdatesRequest = subscribeToUpdates,
    setInterval: setIntervalRequest = setInterval,
    clearInterval: clearIntervalRequest = clearInterval,
  } = dependencies;
  const refreshPacketsRequest = dependencies.refreshPackets
    || (dependencies.refreshWorkbench
      ? async () => (await refreshWorkbenchRequest()).packets
      : getPackets);
  const hasDedicatedPacketRefresh = Boolean(dependencies.refreshPackets);
  const sendSectionNode = documentRef.querySelector('#send-section');
  const resultNode = documentRef.querySelector('#result');
  let activeMode = 'listen';
  let interval;
  let unsubscribe;
  let currentStatus;
  let refreshInFlight;
  let refreshQueued = false;
  let statusRevision = 0;
  let modeChangeInFlight = false;

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
    onModeChange: async (mode, forwardAddress) => {
      if (modeChangeInFlight) return;
      modeChangeInFlight = true;
      statusRevision += 1;
      try {
        const response = await setModeRequest(mode, forwardAddress);
        if (currentStatus) {
          applyStatus({
            ...currentStatus,
            mode: response?.mode ?? mode,
            gsmtapForward: response?.forwardAddress ?? (mode === 'listen' ? null : currentStatus.gsmtapForward),
          });
        }
        const data = await refreshWorkbenchRequest();
        applyStatus(data.status);
        packetHistory.updatePackets(data.packets);
      } catch (error) {
        await refreshWorkbenchRequest().then(data => {
          applyStatus(data.status);
          packetHistory.updatePackets(data.packets);
        });
        throw error;
      } finally {
        modeChangeInFlight = false;
      }
    },
    onModeError: error => showResult(`Unable to change mode: ${error.message}`),
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
    const modeChanged = currentStatus?.mode !== status.mode;
    currentStatus = status;
    activeMode = status.mode;
    modePanel.applyStatus(status);
    packetHistory.setMode(status.mode);
    sendSectionNode.hidden = activeMode !== 'modify';
    if (modeChanged) modifyPanel.resetSelection();
  }

  async function performRefresh() {
    const requestedStatusRevision = statusRevision;
    try {
      if (!currentStatus) {
        const data = await refreshWorkbenchRequest();
        if (requestedStatusRevision === statusRevision) applyStatus(data.status);
        packetHistory.updatePackets(data.packets);
        return;
      }
      if (hasDedicatedPacketRefresh) {
        packetHistory.updatePackets(await refreshPacketsRequest());
      } else {
        const data = await refreshWorkbenchRequest();
        if (requestedStatusRevision === statusRevision) applyStatus(data.status);
        packetHistory.updatePackets(data.packets);
      }
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
