import { jest } from '@jest/globals';
import { getPackets, getStatus, previewModification, refreshWorkbench, replayPacket, sendModification, subscribeToUpdates } from '../../static/services/api.js';

jest.useFakeTimers();

test('uses the expected API paths and request payloads', async () => {
  global.fetch = jest.fn(async () => ({ ok: true, json: async () => ({}), text: async () => 'ok' }));
  await getStatus();
  await getPackets(12);
  await replayPacket(7);
  await previewModification(7, { arfcn: 42 });
  await sendModification(7, { arfcn: 43 });
  expect(fetch.mock.calls.map(([path]) => path)).toEqual([
    '/api/status', '/api/packets?limit=12', '/api/packets/7/replay',
    '/api/packets/7/modify-preview', '/api/packets/7/modify-send',
  ]);
  expect(fetch.mock.calls[3][1]).toMatchObject({ method: 'POST', body: JSON.stringify({ arfcn: 42 }) });
});

test('refreshes status and packets together', async () => {
  global.fetch = jest.fn(async path => ({
    ok: true,
    json: async () => path === '/api/status' ? { mode: 'listen' } : [{ id: 1 }],
  }));
  await expect(refreshWorkbench()).resolves.toEqual({ status: { mode: 'listen' }, packets: [{ id: 1 }] });
});

test('debounces event-driven refreshes and supports cleanup', () => {
  let source;
  class FakeEventSource {
    constructor(path) {
      this.path = path;
      this.close = jest.fn();
      source = this;
    }
  }
  const onUpdate = jest.fn();
  const unsubscribe = subscribeToUpdates(onUpdate, { EventSourceImpl: FakeEventSource, delay: 10 });

  expect(source.path).toBe('/api/events');
  source.onmessage();
  source.onmessage();
  expect(onUpdate).not.toHaveBeenCalled();
  jest.advanceTimersByTime(10);
  expect(onUpdate).toHaveBeenCalledTimes(1);
  unsubscribe();
  expect(source.close).toHaveBeenCalled();
});
