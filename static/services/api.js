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

export async function getPackets(limit = 500) {
  return (await request(`/api/packets?limit=${limit}`)).json();
}

export async function replayPacket(id) {
  return request(`/api/packets/${id}/replay`, 'POST');
}

export async function previewModification(id, payload) {
  return request(`/api/packets/${id}/modify-preview`, 'POST', payload);
}

export async function sendModification(id, payload) {
  return request(`/api/packets/${id}/modify-send`, 'POST', payload);
}
