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
import { defaultPacketFilters, filterPackets, isFilterActive } from '../components/filters.js';

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
  const packetsNode = documentRef.querySelector('#packets');
  const clearFiltersNode = documentRef.querySelector('#clear-filters');
  const clearHistoryNode = documentRef.querySelector('#clear-history');
  const forwardSelectedNode = documentRef.querySelector('#forward-selected');
  const clearHistoryDialog = documentRef.querySelector('#clear-history-dialog');
  const clearHistorySkipNode = documentRef.querySelector('#clear-history-skip');
  const confirmClearHistoryNode = documentRef.querySelector('#confirm-clear-history');
  const cancelClearHistoryNode = documentRef.querySelector('#cancel-clear-history');
  const sendSectionNode = documentRef.querySelector('#send-section');
  const resultNode = documentRef.querySelector('#result');
  let activeMode = 'listen';
  let packets = [];
  let selectedPacketId = null;
  const selectedRelayPacketIds = new Set();
  let forwardInFlight = false;
  let interval;
  let unsubscribe;
  let currentStatus;
  let refreshInFlight;
  let refreshQueued = false;
  let statusRevision = 0;
  let packetFilters = { ...defaultPacketFilters };
  let openFilterField = null;
  const clearHistoryPreferenceKey = 'gsmtap.clear-history.skip-confirmation';

  function showResult(text) {
    resultNode.textContent = text;
  }

  async function forwardSelectedPackets() {
    if (forwardInFlight || selectedRelayPacketIds.size === 0 || activeMode !== 'relay') return;
    forwardInFlight = true;
    if (forwardSelectedNode) forwardSelectedNode.disabled = true;
    try {
      const packetIds = Array.from(selectedRelayPacketIds);
      const response = await forwardPacketsRequest(packetIds);
      if (!response.ok) {
        const errorText = await response.text();
        showResult(`Forwarding failed (${response.status}): ${errorText}`);
        return;
      }
      const data = await response.json();
      const results = data.results || [];
      let sentCount = 0;
      let failedCount = 0;
      let notFoundCount = 0;
      for (const result of results) {
        if (result.status === 'sent') {
          selectedRelayPacketIds.delete(result.packetId);
          sentCount++;
        } else if (result.status === 'not found') {
          selectedRelayPacketIds.delete(result.packetId);
          notFoundCount++;
        } else {
          failedCount++;
        }
      }
      await refresh();
      if (failedCount > 0 && sentCount > 0) {
        showResult(`Forwarded ${sentCount} packet(s), ${failedCount} failed.`);
      } else if (failedCount > 0) {
        showResult(`Forwarding failed for ${failedCount} packet(s).`);
      } else if (notFoundCount > 0 && sentCount > 0) {
        showResult(`Forwarded ${sentCount} packet(s), ${notFoundCount} not found.`);
      } else if (notFoundCount > 0) {
        showResult(`${notFoundCount} packet(s) not found.`);
      } else {
        showResult(`Successfully forwarded ${sentCount} packet(s).`);
      }
    } catch (error) {
      showResult(`Unable to forward packets: ${error.message}`);
    } finally {
      forwardInFlight = false;
      render();
    }
  }

  async function clearHistory() {
    clearHistoryNode.disabled = true;
    try {
      await clearPacketHistoryRequest();
      packets = [];
      selectedPacketId = null;
      selectedRelayPacketIds.clear();
      packetFilters = { ...defaultPacketFilters };
      openFilterField = null;
      render();
    } catch (error) {
      clearHistoryNode.disabled = false;
      showResult(`Unable to clear packet history: ${error.message}`);
    }
  }

  function openClearHistoryConfirmation() {
    let skipConfirmation = false;
    try {
      skipConfirmation = documentRef.defaultView?.localStorage.getItem(clearHistoryPreferenceKey) === 'true';
    } catch {
      skipConfirmation = false;
    }
    if (skipConfirmation) {
      void clearHistory();
      return;
    }
    clearHistorySkipNode.checked = false;
    if (typeof clearHistoryDialog.showModal === 'function') clearHistoryDialog.showModal();
    else clearHistoryDialog.setAttribute('open', '');
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

  function render() {
    const focusedTextFilter = documentRef.activeElement?.classList.contains('packet-column-search')
      ? documentRef.activeElement.closest('.packet-column-filter')?.dataset.filterField
      : null;
    const filteredPackets = filterPackets(packets, packetFilters);
    modePanel.render(filteredPackets, selectedPacketId, {
      filters: packetFilters,
      hasPackets: packets.length > 0,
      openFilter: openFilterField,
      selectedPacketIds: selectedRelayPacketIds,
      onSelectionChange: (id, selected) => {
        if (selected) selectedRelayPacketIds.add(id);
        else selectedRelayPacketIds.delete(id);
        render();
      },
      onSelectAllChange: (ids, selectAll) => {
        if (selectAll) {
          for (const id of ids) selectedRelayPacketIds.add(id);
        } else {
          for (const id of ids) selectedRelayPacketIds.delete(id);
        }
        render();
      },
      onFilterOpenChange: field => { openFilterField = field; },
      onFilterChange: ({ field, value }) => {
        packetFilters = { ...packetFilters, [field]: value };
        render();
      },
    });
    if (clearFiltersNode) {
      clearFiltersNode.hidden = !isFilterActive(packetFilters, activeMode);
      clearFiltersNode.onclick = () => {
        packetFilters = { ...defaultPacketFilters };
        openFilterField = null;
        render();
      };
    }
    if (forwardSelectedNode) {
      forwardSelectedNode.hidden = activeMode !== 'relay';
      forwardSelectedNode.disabled = selectedRelayPacketIds.size === 0 || forwardInFlight;
      forwardSelectedNode.textContent = `Forward selected (${selectedRelayPacketIds.size})`;
      forwardSelectedNode.onclick = () => { void forwardSelectedPackets(); };
    }
    if (clearHistoryNode) {
      clearHistoryNode.hidden = false;
      clearHistoryNode.disabled = packets.length === 0;
      clearHistoryNode.onclick = openClearHistoryConfirmation;
    }
    if (focusedTextFilter) {
      const search = packetsNode.querySelector(
        `[data-filter-field="${focusedTextFilter}"] .packet-column-search`,
      );
      if (search) {
        search.focus();
        search.setSelectionRange(search.value.length, search.value.length);
      }
    }
  }

  confirmClearHistoryNode?.addEventListener('click', event => {
    event.preventDefault();
    if (clearHistorySkipNode.checked) {
      try { documentRef.defaultView?.localStorage.setItem(clearHistoryPreferenceKey, 'true'); } catch { /* Storage may be unavailable. */ }
    }
    clearHistoryDialog.close?.();
    void clearHistory();
  });
  cancelClearHistoryNode?.addEventListener('click', event => {
    event.preventDefault();
    clearHistoryDialog.close?.();
  });

  function applyStatus(status) {
    const modeChanged = activeMode !== status.mode;
    currentStatus = status;
    activeMode = status.mode;
    if (modeChanged) openFilterField = null;
    if (activeMode !== 'modify') {
      packetFilters = { ...packetFilters, direction: null, modified: null };
    }
    modePanel.applyStatus(status);
    sendSectionNode.hidden = activeMode !== 'modify';
  }

  async function performRefresh() {
    const requestedStatusRevision = statusRevision;
    try {
      const data = await refreshWorkbenchRequest();
      if (requestedStatusRevision === statusRevision) applyStatus(data.status);
      packets = data.packets;
      const currentPacketIds = new Set(packets.map(p => p.id));
      for (const id of Array.from(selectedRelayPacketIds)) {
        if (!currentPacketIds.has(id)) {
          selectedRelayPacketIds.delete(id);
        }
      }
      render();
    } catch (error) {
      packetsNode.replaceChildren();
      const message = documentRef.createElement('p');
      message.className = 'error';
      message.textContent = `Unable to load packet history: ${error.message}`;
      packetsNode.append(message);
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
