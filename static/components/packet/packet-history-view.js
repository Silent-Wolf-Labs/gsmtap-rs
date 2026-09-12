import { isFilterActive } from '../filters.js';

export function createPacketHistoryView({ documentRef = document, modePanel } = {}) {
  const packetsNode = documentRef.querySelector('#packets');
  const clearFiltersNode = documentRef.querySelector('#clear-filters');
  const forwardSelectedNode = documentRef.querySelector('#forward-selected');
  const clearHistoryNode = documentRef.querySelector('#clear-history');

  function render({
    packets,
    selectedPacketId,
    mode,
    filters,
    hasPackets,
    openFilter,
    selectedPacketIds,
    forwardInFlight,
    clearHistoryInFlight,
    onSelectionChange,
    onSelectAllChange,
    onFilterOpenChange,
    onFilterChange,
    onClearFilters,
    onForward,
    onClearHistory,
  }) {
    const focusedTextFilter = documentRef.activeElement?.classList.contains('packet-column-search')
      ? documentRef.activeElement.closest('.packet-column-filter')?.dataset.filterField
      : null;

    modePanel.setMode?.(mode);
    modePanel.render(packets, selectedPacketId, {
      filters,
      hasPackets,
      openFilter,
      selectedPacketIds,
      onSelectionChange,
      onSelectAllChange,
      onFilterOpenChange,
      onFilterChange,
    });

    if (clearFiltersNode) {
      clearFiltersNode.hidden = !isFilterActive(filters, mode);
      clearFiltersNode.onclick = onClearFilters;
    }
    if (forwardSelectedNode) {
      forwardSelectedNode.hidden = mode !== 'relay';
      forwardSelectedNode.disabled = selectedPacketIds.size === 0 || forwardInFlight;
      forwardSelectedNode.textContent = `Forward selected (${selectedPacketIds.size})`;
      forwardSelectedNode.onclick = onForward;
    }
    if (clearHistoryNode) {
      clearHistoryNode.hidden = false;
      clearHistoryNode.disabled = !hasPackets || clearHistoryInFlight;
      clearHistoryNode.onclick = onClearHistory;
    }
    if (focusedTextFilter) {
      const search = packetsNode?.querySelector(
        `[data-filter-field="${focusedTextFilter}"] .packet-column-search`,
      );
      if (search) {
        search.focus();
        search.setSelectionRange(search.value.length, search.value.length);
      }
    }
  }

  function renderError(error) {
    packetsNode?.replaceChildren();
    if (!packetsNode) return;
    const message = documentRef.createElement('p');
    message.className = 'error';
    message.textContent = `Unable to load packet history: ${error.message}`;
    packetsNode.append(message);
  }

  return { render, renderError };
}