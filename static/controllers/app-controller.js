import {
  getStatus,
  getPackets,
  clearPacketHistory,
  forwardPackets,
  previewModification,
  sendModification,
  setCapturePaused,
  setListenAddress,
  setMode,
} from '../services/api.js';
import { subscribeToUpdates } from '../services/events.js';
import { createModifyPanel } from '../components/panels/modify-panel.js';
import { createModePanel } from '../components/panels/mode-panel.js';
import { createPacketHistoryView } from '../components/packet/packet-history-view.js';
import { createPacketHistoryController } from './packet-history-controller.js';
import { createConversionController } from './conversion-controller.js';
import { createNavigationController } from './navigation-controller.js';
import { createConversionPanels } from '../components/conversion/conversion-panel.js';

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
    setListenAddress: setListenAddressRequest = setListenAddress,
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
  const navigation = createNavigationController(documentRef);
  let conversionViews;
  const conversions = createConversionController({
    service: dependencies.conversionService,
    onChange: (tool, state) => conversionViews?.get(tool)?.update(state),
  });
  conversionViews = createConversionPanels({ documentRef, controller: conversions });

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
    onListenChange: async listenAddress => {
      statusRevision += 1;
      const response = await setListenAddressRequest(listenAddress);
      if (currentStatus) {
        applyStatus({
          ...currentStatus,
          gsmtapListen: response?.listenAddress ?? listenAddress,
        });
      }
      const data = await refreshWorkbenchRequest();
      applyStatus(data.status);
      packetHistory.updatePackets(data.packets);
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
    navigation.start();
    refresh();
    interval = setIntervalRequest(refresh, 1000);
    unsubscribe = subscribeToUpdatesRequest(refresh);
  }

  function stop() {
    navigation.stop();
    conversions.stop();
    if (interval) clearIntervalRequest(interval);
    if (unsubscribe) unsubscribe();
    interval = undefined;
    unsubscribe = undefined;
  }

  return { start, stop, refresh };
}
