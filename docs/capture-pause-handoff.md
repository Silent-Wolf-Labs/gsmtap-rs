# Capture Pause/Resume Handoff

## Goal

Add an explicit capture pause/resume control. The UDP socket must keep receiving traffic while paused, but incoming packets received during the paused period must not be retained in packet history or appear in the packet table. Add deterministic test support that proves the receive-time pause decision wins over the later worker state.

## Approved behavior

- `receiveState` remains `"listening"` at all times while the UDP receiver is operating.
- Add `capturePaused: bool` to the status response. `true` means incoming traffic is processed but is not retained in RX history.
- Pause applies only to incoming RX capture. Explicit TX records created by encode/send, modify/send, and replay remain recorded.
- In relay mode, paused capture does not stop decoding or forwarding.
- Decode, queue-drop, and relay forwarding counters continue to reflect real traffic while paused.
- Add `captureSkipped`: every datagram received while paused increments this counter, including a datagram subsequently dropped because the processing queue is full.
- The status UI uses `Capture paused` and `Skipped while paused`; do not use wording that implies paused packets were captured.
- Pausing suppresses only the packet-history SSE event that would normally announce an RX record. It does not create a general SSE-off switch.

## Backend architecture

Create a shared `CaptureControl` with an atomic paused flag.

```
Workbench startup
       |
       +-- CaptureControl
             +-- HTTP state
             +-- UDP receiver

UDP socket
   |
   +-- received += 1
   |
   +-- snapshot CaptureControl
   |     +-- if paused: captureSkipped += 1
   |
   +-- queue QueuedDatagram { bytes, peer, timestampMs, captureEnabled }
   |     +-- queue failure: ingressDropped += 1
   |
   +-- packet worker
         +-- decode and update decode/error counters
         +-- relay forwarding, when enabled
         +-- if captureEnabled: store RX PacketRecord and emit its packet-history SSE event

Explicit TX
   |
   +-- send packet
   +-- store TX PacketRecord
         +-- unaffected by CaptureControl
```

Pass `CaptureControl` only to HTTP state and the UDP receiver. Do not pass it to the processing worker. Add `capture_enabled: bool` to `QueuedDatagram`; the receiver snapshots `!capture_control.is_paused()` immediately after incrementing `received`, increments `captureSkipped` when false, and queues that immutable decision with the datagram.

The worker must use only `QueuedDatagram.capture_enabled` to decide whether it stores the RX record. This prevents resuming capture while a paused datagram is queued from causing that datagram to enter history.

Add `capture_skipped: AtomicU64` to `RuntimeCounters` and `capture_skipped: u64` to `RuntimeStats`. The counter increment occurs before `try_send`, so a paused queue-dropped datagram increments both `captureSkipped` and `ingressDropped`.

Provide `PUT /api/capture` with this request and response:

```json
{ "paused": true }
```

```json
{ "capturePaused": true }
```

The endpoint is idempotent and works in listen, relay, and modify modes. Extend `AppState` with the shared control. Startup creates one control and supplies it to both the router and UDP receiver. Keep a test-only constructor/helper that accepts an injected control so router and receiver tests share the same instance.

## UI behavior

Add `setCapturePaused(paused)` to the API service, using `PUT /api/capture`.

Render an accessible control in the status card:

- `Pause capture` when `capturePaused` is false.
- `Resume capture` when `capturePaused` is true.
- `aria-pressed` equals `capturePaused`.
- Disable the button until the update request completes.
- After success, refresh status and packet history.
- On failure, re-enable the button and display an accessible status-card error message.

Status display includes `Capture paused` and `Skipped while paused`. The status card receives a callback from the application controller through the mode panel; it does not call the API service directly.

## Test apparatus and validation

### Interactive traffic generator

Add or maintain `tests/traffic/continuous.py` as the operator-facing test tool. It must use only Python's standard library and send valid GSMTAP v2 UDP packets continuously until Ctrl-C. The default target is `127.0.0.1:4729` with a 100 ms interval.

Example:

```sh
python3 tests/traffic/continuous.py
```

Useful controls:

```sh
python3 tests/traffic/continuous.py --interval 0.02 --payload-size 32
python3 tests/traffic/continuous.py --duration 10
python3 tests/traffic/continuous.py --host 192.0.2.10 --port 4729
```

Each packet should have a valid 16-byte GSMTAP header and changing sequence/frame/payload values so the packet table visibly updates while capture is active. The script must report progress and stop cleanly on Ctrl-C.

Add a test harness path that constructs one injected `CaptureControl`, passes it to the test router and UDP receiver, and can hold worker processing until the test explicitly releases it. Use it to prove the receive-time snapshot contract.

Add Rust tests for:

- active capture records an RX packet;
- paused capture increments `received` and `captureSkipped` without creating history or a packet-history SSE event;
- paused capture in relay mode still forwards the datagram and increments forwarding counters;
- a paused queue-dropped datagram increments both `captureSkipped` and `ingressDropped`;
- repeated `PUT { "paused": true }` responses remain `capturePaused: true`, and resume returns false;
- pausing capture does not suppress TX records from modify/send and replay;
- boundary behavior: pause, receive datagram A, resume before the worker processes A, release A, assert A is absent; receive B after resume, assert B is stored.

Add UI tests for:

- capture-state labels, skipped metric, button text, and `aria-pressed`;
- successful pause and resume request flow followed by refresh;
- disabled in-flight button behavior;
- failure feedback and button recovery;
- existing packet-table, mode, polling, and SSE refresh behavior.

Run `cargo fmt --check`, the full Rust suite, and the complete UI test suite.

## Commit split

Keep implementation and tests in separate conventional commits:

1. `feat(ui): add paused packet capture control`
2. `test(ui): cover paused packet capture`

Do not include this handoff document in either implementation commit unless documentation changes are explicitly requested.
