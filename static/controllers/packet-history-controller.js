import { createClearHistoryDialog } from '../components/dialogs/clear-history-dialog.js';
import { defaultPacketFilters, filterPackets } from '../components/filters.js';
import { forwardPackets, clearPacketHistory } from '../services/api.js';

export function createPacketHistoryController({
  documentRef = document,
  view,
  forwardPackets: forwardPacketsRequest = forwardPackets,
  clearPacketHistory: clearPacketHistoryRequest = clearPacketHistory,
  requestRefresh,
  showResult = (_text) => {},
} = {}) {
  let mode = 'listen';
  let packets = [];
  let selectedPacketId = null;
  const selectedRelayPacketIds = new Set();
  let forwardInFlight = false;
  let clearHistoryInFlight = false;
  let packetFilters = { ...defaultPacketFilters };
  let openFilterField = null;

  function render() {
    view.render({
      packets: filterPackets(packets, packetFilters),
      selectedPacketId,
      mode,
      filters: packetFilters,
      hasPackets: packets.length > 0,
      openFilter: openFilterField,
      selectedPacketIds: selectedRelayPacketIds,
      forwardInFlight,
      clearHistoryInFlight,
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
      onClearFilters: () => {
        packetFilters = { ...defaultPacketFilters };
        openFilterField = null;
        render();
      },
      onForward: () => { void forwardSelectedPackets(); },
      onClearHistory: clearHistoryDialog.open,
    });
  }

  function updatePackets(nextPackets) {
    packets = nextPackets;
    const currentPacketIds = new Set(packets.map(packet => packet.id));
    for (const id of Array.from(selectedRelayPacketIds)) {
      if (!currentPacketIds.has(id)) selectedRelayPacketIds.delete(id);
    }
    render();
  }

  function setMode(nextMode) {
    const modeChanged = mode !== nextMode;
    mode = nextMode;
    if (modeChanged) openFilterField = null;
    if (mode !== 'modify') {
      packetFilters = { ...packetFilters, direction: null, modified: null };
    }
    render();
  }

  function selectPacket(packet) {
    selectedPacketId = packet?.id ?? null;
  }

  async function forwardSelectedPackets() {
    if (forwardInFlight || selectedRelayPacketIds.size === 0 || mode !== 'relay') return;
    forwardInFlight = true;
    render();
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
      await requestRefresh();
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
    clearHistoryInFlight = true;
    render();
    try {
      await clearPacketHistoryRequest();
      packets = [];
      selectedPacketId = null;
      selectedRelayPacketIds.clear();
      packetFilters = { ...defaultPacketFilters };
      openFilterField = null;
      clearHistoryInFlight = false;
      render();
    } catch (error) {
      clearHistoryInFlight = false;
      render();
      showResult(`Unable to clear packet history: ${error.message}`);
    }
  }

  const clearHistoryDialog = createClearHistoryDialog({ documentRef, onConfirm: clearHistory });

  return {
    updatePackets,
    setMode,
    selectPacket,
    render,
    renderError: view.renderError,
  };
}