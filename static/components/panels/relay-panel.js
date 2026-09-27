import { relayForwardingState } from '../../models/packet-model.js';

export { relayForwardingState };

export const relayCapability = Object.freeze({
  mode: 'relay',
  inspect: true,
  modify: false,
  replay: false,
  forwarding: true,
  actions: Object.freeze({}),
});
