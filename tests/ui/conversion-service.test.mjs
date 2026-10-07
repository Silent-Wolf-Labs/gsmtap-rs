import { jest } from '@jest/globals';
import { createConversionService } from '../../static/services/conversion-service.js';

test.each(['hexparse', 'bcd', 'bits', 'base64'])('posts %s requests and returns conversion failures as data', async tool => {
  const data = { operation: 'decode', success: false, error: { code: 'invalidInput' }, finalDestinationHex: 'AA' };
  const fetchImpl = jest.fn(async () => ({ ok: true, json: async () => data }));
  const service = createConversionService({ available: true, enabledTools: [tool], fetchImpl });
  const signal = new AbortController().signal;
  expect(await service.convert(tool, { operation: 'decode' }, { signal })).toEqual(data);
  expect(fetchImpl).toHaveBeenCalledWith(`/api/conversions/${tool}`, { method: 'POST', headers: { 'content-type': 'application/json' }, body: '{"operation":"decode"}', signal });
});

test('unavailable service and unknown tools never fetch', async () => {
  const fetchImpl = jest.fn(); const service = createConversionService({ fetchImpl, available: false });
  await expect(service.convert('hexparse', {})).rejects.toThrow('not available');
  await expect(service.convert('../status', {})).rejects.toThrow('Unknown conversion tool');
  expect(fetchImpl).not.toHaveBeenCalled();
});

test.each(['Request too large', ''])('reports HTTP errors with body or status: %s', async body => {
  const service = createConversionService({ available: true, fetchImpl: jest.fn(async () => ({ ok: false, status: 413, text: async () => body })) });
  await expect(service.convert('hexparse', {})).rejects.toThrow(body || '413');
});
