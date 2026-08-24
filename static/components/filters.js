import { createSelectControl } from './select-control.js';

const definitions = [
  { id: 'direction', label: 'Direction', options: [['all', 'All'], ['RX', 'RX'], ['TX', 'TX']] },
  { id: 'parse', label: 'Decode', options: [['all', 'All'], ['success', 'Success'], ['error', 'Error']] },
  { id: 'modified', label: 'Modified', options: [['all', 'All'], ['yes', 'Modified'], ['no', 'Unmodified']] },
];

export function visibleFilterIds(mode) {
  return definitions
    .filter(definition =>
      !(definition.id === 'direction' && mode !== 'modify') &&
      !(definition.id === 'modified' && mode !== 'modify'))
    .map(definition => definition.id);
}

export function filterPackets(packets, filters) {
  const direction = filters.direction ?? 'all';
  const parse = filters.parse ?? 'all';
  const modified = filters.modified ?? 'all';
  return packets.filter(packet =>
    (direction === 'all' || packet.direction === direction) &&
    (parse === 'all' || (parse === 'success') === !packet.parseError) &&
    (modified === 'all' || (modified === 'yes') === packet.modified));
}

export function createFilters(node, onChange) {
  let mode = 'listen';
  let controls = new Map();

  function mount() {
    node.replaceChildren();
    controls = new Map();
    for (const definition of definitions) {
      if (!visibleFilterIds(mode).includes(definition.id)) continue;
      const control = createSelectControl({
        id: `filter-${definition.id}`,
        label: definition.label,
        options: definition.options,
        onChange,
      });
      node.append(control.node);
      controls.set(definition.id, control);
    }
  }

  mount();

  return {
    read() {
      return Object.fromEntries([...controls].map(([id, control]) => [id, control.value()]));
    },
    filter(packets) {
      return filterPackets(packets, this.read());
    },
    setMode(nextMode) {
      if (mode === nextMode) return;
      mode = nextMode;
      mount();
    },
  };
}
