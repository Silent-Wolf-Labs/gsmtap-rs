export function createModifyState() {
  return { selectedPacket: null, pendingPayload: null };
}

export function selectModifyPacket(state, packet) {
  return { ...state, selectedPacket: packet, pendingPayload: null };
}

export function setPendingPayload(state, payload) {
  return { ...state, pendingPayload: payload };
}

export function clearPendingPayload(state) {
  return { ...state, pendingPayload: null };
}
