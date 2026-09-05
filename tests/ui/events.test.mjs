import { jest } from '@jest/globals';
import { subscribeToUpdates } from '../../static/services/events.js';

test('debounces event updates using the configured delay', () => {
  jest.useFakeTimers();
  const onUpdate = jest.fn();
  const EventSourceImpl = jest.fn(function EventSource() {
    this.close = jest.fn();
  });
  const unsubscribe = subscribeToUpdates(onUpdate, { EventSourceImpl, delay: 100 });
  const events = EventSourceImpl.mock.instances[0];

  events.onmessage();
  events.onmessage();
  jest.advanceTimersByTime(99);
  expect(onUpdate).not.toHaveBeenCalled();
  jest.advanceTimersByTime(1);
  expect(onUpdate).toHaveBeenCalledTimes(1);

  unsubscribe();
  jest.useRealTimers();
});

test('uses the default delay and closes the event stream on unsubscribe', () => {
  jest.useFakeTimers();
  const onUpdate = jest.fn();
  const EventSourceImpl = jest.fn(function EventSource() {
    this.close = jest.fn();
  });
  const unsubscribe = subscribeToUpdates(onUpdate, { EventSourceImpl });
  const events = EventSourceImpl.mock.instances[0];

  expect(EventSourceImpl).toHaveBeenCalledWith('/api/events');
  events.onmessage();
  jest.advanceTimersByTime(249);
  expect(onUpdate).not.toHaveBeenCalled();
  jest.advanceTimersByTime(1);
  expect(onUpdate).toHaveBeenCalledTimes(1);
  events.onmessage();
  unsubscribe();
  jest.runOnlyPendingTimers();

  expect(events.close).toHaveBeenCalledTimes(1);
  expect(onUpdate).toHaveBeenCalledTimes(1);
  jest.useRealTimers();
});

test('uses the global EventSource default and cancels a pending update', () => {
  jest.useFakeTimers();
  const onUpdate = jest.fn();
  const EventSourceImpl = jest.fn(function EventSource() { this.close = jest.fn(); });
  global.EventSource = EventSourceImpl;

  const unsubscribe = subscribeToUpdates(onUpdate);
  const events = EventSourceImpl.mock.instances[0];
  events.onmessage();
  unsubscribe();
  jest.runOnlyPendingTimers();

  expect(EventSourceImpl).toHaveBeenCalledWith('/api/events');
  expect(events.close).toHaveBeenCalledTimes(1);
  expect(onUpdate).not.toHaveBeenCalled();
  jest.useRealTimers();
});
