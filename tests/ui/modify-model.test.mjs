import { ModifyModel } from '../../static/models/modify-model.js';

function packet(overrides = {}) {
  return { id: 1, decoded: {
    version: 2, headerLengthWords: 4, messageType: 1, timeslot: 0, arfcn: 1,
    signalDbm: 0, snrDb: 0, frameNumber: 1, subtype: 0, antennaNumber: 0,
    subSlot: 0, reserved: 0, extensionHex: 'CA FE', payloadHex: 'AA BB', ...overrides,
  } };
}

test('derives changes from baseline and current values', () => {
  const model = new ModifyModel();
  model.selectPacket(packet());
  expect(model.hasChanges).toBe(false);
  model.updateField('arfcn', '2');
  expect(model.hasChanges).toBe(true);
  model.updateField('arfcn', 1);
  expect(model.hasChanges).toBe(false);
});

test('compares valid hex semantically and invalid hex textually', () => {
  const model = new ModifyModel();
  model.selectPacket(packet());
  model.updateField('payloadHex', 'aa  bb');
  expect(model.hasChanges).toBe(false);
  model.updateField('payloadHex', 'AA BC');
  expect(model.hasChanges).toBe(true);
  model.updateField('payloadHex', 'AA BB');
  model.updateField('extensionHex', 'invalid');
  expect(model.hasChanges).toBe(true);
});

test('owns and invalidates pending preview payloads', () => {
  const model = new ModifyModel();
  model.selectPacket(packet());
  model.updateField('arfcn', 2);
  const payload = model.toRequestPayload();
  model.setPendingPayload(payload);
  expect(model.pendingPayload).toEqual(payload);
  model.updateField('arfcn', 3);
  expect(model.pendingPayload).toBeNull();
});

test('notifies subscribers with model change reasons and resets completely', () => {
  const model = new ModifyModel();
  const reasons = [];
  model.subscribe((current, reason) => reasons.push([reason, current.hasChanges]));
  model.selectPacket(packet());
  model.updateField('arfcn', 2);
  model.setPendingPayload(model.toRequestPayload());
  model.reset();
  expect(reasons).toEqual([
    ['selection', false], ['field', true], ['pending', true], ['reset', false],
  ]);
  expect(model.selectedPacket).toBeNull();
  expect(model.values).toBeNull();
  expect(model.pendingPayload).toBeNull();
});
