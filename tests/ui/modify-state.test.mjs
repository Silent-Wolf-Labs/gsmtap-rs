import { clearPendingPayload, createModifyState, selectModifyPacket, setPendingPayload } from '../../static/components/modify/modify-state.js';

test('selecting a packet clears stale pending changes', () => {
  const packet = { id: 1 };
  const state = setPendingPayload(createModifyState(), { arfcn: 12 });

  expect(selectModifyPacket(state, packet)).toEqual({ selectedPacket: packet, pendingPayload: null });
});

test('pending changes can be set and cleared independently', () => {
  const state = createModifyState();
  const pending = { arfcn: 12 };

  expect(setPendingPayload(state, pending)).toEqual({ selectedPacket: null, pendingPayload: pending });
  expect(clearPendingPayload(setPendingPayload(state, pending))).toEqual(state);
});
