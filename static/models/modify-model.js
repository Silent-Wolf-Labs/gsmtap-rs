import { MODIFY_FIELDS } from '../components/modify/modify-fields.js';

const FIELD_KEYS = MODIFY_FIELDS.map(({ key }) => key);
const NUMERIC_KEYS = new Set(MODIFY_FIELDS.filter(field => field.type === 'number').map(({ key }) => key));
const HEX_KEYS = new Set(['extensionHex', 'payloadHex']);

function editableValues(decoded = {}) {
  return Object.fromEntries(FIELD_KEYS.map(key => [key, decoded[key] ?? (NUMERIC_KEYS.has(key) ? 0 : '')]));
}

function equalHex(left, right) {
  const normalize = value => {
    const text = String(value ?? '');
    const compact = text.replace(/\s/g, '');
    return /^[0-9a-f]*$/i.test(compact) && compact.length % 2 === 0 ? compact.toLowerCase() : null;
  };
  const normalizedLeft = normalize(left);
  const normalizedRight = normalize(right);
  return normalizedLeft === null || normalizedRight === null
    ? String(left ?? '') === String(right ?? '')
    : normalizedLeft === normalizedRight;
}

function valuesEqual(left, right) {
  return FIELD_KEYS.every(key => HEX_KEYS.has(key)
    ? equalHex(left[key], right[key])
    : left[key] === right[key]);
}

export class ModifyModel {
  constructor() {
    this.baseline = null;
    this.values = null;
    this.selectedPacket = null;
    this.pendingPayload = null;
    this.listeners = new Set();
  }

  get hasChanges() {
    return this.baseline !== null && !valuesEqual(this.baseline, this.values);
  }

  subscribe(listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  selectPacket(packet) {
    this.selectedPacket = packet;
    this.baseline = editableValues(packet.decoded);
    this.values = { ...this.baseline };
    this.pendingPayload = null;
    this.notify('selection');
  }

  updateField(key, value) {
    if (!this.values || !FIELD_KEYS.includes(key)) return;
    const nextValue = NUMERIC_KEYS.has(key)
      ? (value === '' ? '' : Number(value))
      : String(value ?? '');
    if (this.values[key] === nextValue) return;
    this.values = { ...this.values, [key]: nextValue };
    this.pendingPayload = null;
    this.notify('field');
  }

  toRequestPayload() {
    return this.values ? { ...this.values } : null;
  }

  setPendingPayload(payload) {
    this.pendingPayload = payload ? { ...payload } : null;
    this.notify('pending');
  }

  reset() {
    this.baseline = null;
    this.values = null;
    this.selectedPacket = null;
    this.pendingPayload = null;
    this.notify('reset');
  }

  notify(reason) {
    for (const listener of this.listeners) listener(this, reason);
  }
}
