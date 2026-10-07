import { jest } from '@jest/globals';
import { createConversionController } from '../../static/controllers/conversion-controller.js';

const response = { operation: 'parse', success: true, returnValue: 1, finalDestinationHex: 'CA' };
function deferred() { let resolve, reject; const promise = new Promise((yes, no) => { resolve = yes; reject = no; }); return { promise, resolve, reject }; }
function setup(convert = jest.fn(async () => response)) {
  const service = { available: true, convert }; const onChange = jest.fn();
  return { service, onChange, controller: createConversionController({ service, onChange }) };
}

test('submits normalized models, stores output separately, and preserves other tools', async () => {
  const { controller, service, onChange } = setup();
  controller.update('hexparse', 'source', 'CA'); controller.update('hexparse', 'destination.capacity', '1');
  controller.update('bcd', 'source', '123');
  await controller.submit('hexparse');
  expect(service.convert).toHaveBeenCalledWith('hexparse', expect.objectContaining({ source: { encoding: 'text', data: 'CA' }, destination: { capacity: 1, fillByte: 0, initialHex: '' } }), expect.objectContaining({ signal: expect.anything() }));
  expect(controller.getState('hexparse').output.result).toEqual(response);
  expect(controller.getState('bcd').input.values.source).toBe('123');
  expect(onChange).toHaveBeenCalledWith('hexparse', controller.getState('hexparse'));
  controller.update('hexparse', 'source', 'FF');
  expect(controller.getState('hexparse').output.hasResult).toBe(false);
});

test('validation failure prevents a request', async () => {
  const { controller, service } = setup(); controller.update('bits', 'source', 'GG');
  await controller.submit('bits');
  expect(controller.getState('bits').errors).toHaveProperty('source');
  expect(service.convert).not.toHaveBeenCalled();
});

test('unavailable endpoints are not called', async () => {
  const controller = createConversionController({ service: { available: false } });
  expect(controller.available).toBe(false);
  await controller.submit('hexparse');
  expect(controller.getState('hexparse').requestError).toContain('not available');
});

test('disables duplicate requests and restores pending state after failure', async () => {
  const pending = deferred(); const { controller, service } = setup(jest.fn(() => pending.promise));
  const first = controller.submit('hexparse'); await controller.submit('hexparse');
  expect(service.convert).toHaveBeenCalledTimes(1);
  expect(controller.getState('hexparse').pending).toBe(true);
  pending.reject(new Error('Disconnected')); await first;
  expect(controller.getState('hexparse').pending).toBe(false);
  expect(controller.getState('hexparse').requestError).toBe('Disconnected');
});

test.each(['update', 'reset', 'stop'])('%s cancels requests and ignores late results', async action => {
  const pending = deferred(); const { controller, service } = setup(jest.fn(() => pending.promise));
  const request = controller.submit('hexparse'); const signal = service.convert.mock.calls[0][2].signal;
  if (action === 'update') controller.update('hexparse', 'source', 'FF');
  else if (action === 'reset') controller.reset('hexparse');
  else controller.stop();
  expect(signal.aborted).toBe(true);
  pending.resolve(response); await request;
  expect(controller.getState('hexparse').output.hasResult).toBe(false);
  expect(controller.getState('hexparse').pending).toBe(false);
});

test('an old response cannot overwrite a newer request', async () => {
  const old = deferred(); const fresh = deferred();
  const { controller } = setup(jest.fn().mockImplementationOnce(() => old.promise).mockImplementationOnce(() => fresh.promise));
  const first = controller.submit('hexparse'); controller.update('hexparse', 'source', 'FF'); const second = controller.submit('hexparse');
  fresh.resolve({ ...response, finalDestinationHex: 'FF' }); await second;
  old.resolve(response); await first;
  expect(controller.getState('hexparse').output.finalDestinationHex).toBe('FF');
});

test('rejects invalid tool names and catches malformed server responses', async () => {
  const { controller } = setup(jest.fn(async () => ({})));
  expect(() => controller.getState('unknown')).toThrow('Unknown conversion tool');
  await controller.submit('hexparse');
  expect(controller.getState('hexparse').requestError).toBe('Invalid conversion response.');
});

test('stopping preserves settled results and input for a later restart', async () => {
  const { controller } = setup(); controller.update('hexparse', 'source', 'CA');
  await controller.submit('hexparse'); controller.stop();
  expect(controller.getState('hexparse').input.values.source).toBe('CA');
  expect(controller.getState('hexparse').output.result).toEqual(response);
});
