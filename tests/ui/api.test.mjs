import { jest } from '@jest/globals';
import { getPackets, getStatus, previewModification, replayPacket, sendModification, setCapturePaused } from '../../static/services/api.js';

test('uses the expected API paths and request payloads', async () => {
  global.fetch = jest.fn(async () => ({ ok: true, json: async () => ({}), text: async () => 'ok' }));
  await getStatus();
  await getPackets(12);
  await replayPacket(7);
  await previewModification(7, { arfcn: 42 });
  await sendModification(7, { arfcn: 43 });
  await setCapturePaused(true);
  expect(fetch.mock.calls.map(([path]) => path)).toEqual([
    '/api/status', '/api/packets?limit=12', '/api/packets/7/replay',
    '/api/packets/7/modify-preview', '/api/packets/7/modify-send', '/api/capture',
  ]);
  expect(fetch.mock.calls[3][1]).toMatchObject({ method: 'POST', body: JSON.stringify({ arfcn: 42 }) });
  expect(fetch.mock.calls[5][1]).toMatchObject({ method: 'PUT', body: JSON.stringify({ paused: true }) });
});

test('uses the default packet limit', async () => {
  global.fetch = jest.fn(async () => ({ json: async () => [] }));

  await getPackets();

  expect(fetch).toHaveBeenCalledWith('/api/packets?limit=500', expect.any(Object));
});
