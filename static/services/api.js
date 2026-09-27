async function request(path, method = 'GET', payload = undefined) {
  return fetch(path, {
    method,
    headers: payload ? { 'content-type': 'application/json' } : {},
    body: payload ? JSON.stringify(payload) : undefined,
  });
}

export async function getStatus() {
  return (await request('/api/status')).json();
}

export async function setMode(mode, forwardAddress = undefined) {
  const response = await request('/api/mode', 'PUT', { mode, ...(forwardAddress === undefined ? {} : { forwardAddress }) });
  if (!response.ok) throw new Error((await response.text()) || `mode change failed (${response.status})`);
  return response.json();
}

export async function setListenAddress(listenAddress) {
  const response = await request('/api/listen', 'PUT', { listenAddress });
  if (!response.ok) throw new Error((await response.text()) || `listen address change failed (${response.status})`);
  return response.json();
}

export async function getPackets(limit = 500) {
  return (await request(`/api/packets?limit=${limit}`)).json();
}

export async function clearPacketHistory() {
  const response = await request('/api/packets', 'DELETE');
  if (!response.ok) throw new Error(`packet history clear failed (${response.status})`);
}

export async function setCapturePaused(paused) {
  const response = await request('/api/capture', 'PUT', { paused });
  if (!response.ok) throw new Error(`capture update failed (${response.status})`);
  return response.json();
}

export async function replayPacket(id) {
  return request(`/api/packets/${id}/replay`, 'POST');
}

export async function forwardPackets(packetIds) {
  return request('/api/packets/forward', 'POST', { packetIds });
}

export async function previewModification(id, payload) {
  return request(`/api/packets/${id}/modify-preview`, 'POST', payload);
}

export async function sendModification(id, payload) {
  return request(`/api/packets/${id}/modify-send`, 'POST', payload);
}
