export function createModificationService({ previewModification, sendModification }) {
  return {
    preview: (packetId, payload) => previewModification(packetId, payload),
    send: (packetId, payload) => sendModification(packetId, payload),
  };
}
