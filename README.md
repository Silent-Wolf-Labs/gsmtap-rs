# gsmtap-rs

gsmtap-rs is a Rust GSMTAP library, a web workbench for inspecting and
forwarding live GSMTAP traffic, and a set of direct-call libosmocore reference
harnesses whose recorded outputs serve as a behavioral specification.

- **Library:** GSMTAP header parsing and encoding in `src/gsmtap/`.
- **Workbench:** `gsmtap-workbench`, a single-process web UI that listens for
  GSMTAP over UDP and can relay or edit and resend packets. It can tag received
  packets that exactly match a C-recorded reference vector.
- **Reference harness:** C programs in `reference/` that call public
  libosmocore APIs. Their recorded results are the fixtures in `tests/vectors/`.

gsmtap-rs does not depend on osmocore-rs or libosmocore. The workbench only
listens and forwards, so it works with traffic from osmocore-rs, libosmocore, a
real Osmocom stack or any other GSMTAP sender.

[![Watch the GSMTAP workbench demo](https://img.youtube.com/vi/k0_CIiQXFq0/hqdefault.jpg)](https://youtu.be/k0_CIiQXFq0)

Watch the [GSMTAP workbench demo](https://youtu.be/k0_CIiQXFq0) on YouTube.

## Building and testing

This project requires Rust 1.99 or newer.

```bash
cargo build
cargo test
```

Ordinary tests read the committed fixtures and do not need a C build.

## Live web workbench

Build and run the workbench with:

```bash
cargo run --bin gsmtap-workbench
```

Open `http://localhost:8080`. Use the status card to change the GSMTAP listen
address, choose the operating mode, and set a forward target when Relay or
Modify mode is active.

### Container

The container image is built from `Containerfile` with Buildah and run with
Podman. It exposes TCP port 8080 and UDP port 4729, listens on all interfaces,
runs as an unprivileged user, and matches received packets against the bundled
reference vectors:

```bash
buildah build --layers --tag gsmtap-workbench .
podman run --rm -p 8080:8080 -p 4729:4729/udp gsmtap-workbench
```

The browser only calls the application API. GSMTAP parsing and encoding remain
implemented by the Rust library.

### Matching reference vectors

To check received traffic against the C-recorded GSMTAP vectors, pass the
directory that holds them:

```bash
cargo run --bin gsmtap-workbench -- --reference-vectors tests/vectors
```

Each received packet whose bytes exactly match a recorded vector shows that
vector's case in the **Reference** column, and `/api/status` reports
`referenceVectors` and `stats.referenceMatched`. The container image ships the
vectors and enables this by default. The option is also read from
`GSMTAP_REFERENCE_VECTORS`.

### Operating modes

The workbench has three operating modes. The web UI is available in every
mode: listen and relay provide inspection, while modify additionally provides
explicit edit, preview, and send controls.

![GSMTAP workbench listen, relay, and modify modes](docs/mode_use_cases.svg)

Start the workbench once; it opens in Listen mode at `http://localhost:8080`.
Choose Relay or Modify from the **Workbench Mode** control when needed, then
enter the forward target in the UI. When running the container, publish UDP port
`4729` as shown above.

#### Listen mode: inspect traffic

Listen mode is the default. It receives and retains packets for inspection but
never forwards them. Use it to observe a GSMTAP source safely.

Use **Pause Capture** in the status card to temporarily stop accepting incoming
datagrams; select **Resume Capture** to begin receiving them again. Existing
history remains available while capture is paused.

![Pause and resume capture in listen mode](docs/listen_playPause_demo.gif)

#### Relay mode: inspect and forward selected packets

Relay mode records incoming datagrams without automatically forwarding them.
Select one or more packets in the UI, then choose **Forward selected**. The
workbench sends the original bytes to the forward target configured in the UI;
the target sees the workbench host or container as the UDP source.

![Forward selected packets in relay mode](docs/relay_forward_demo.gif)

#### Modify mode: capture, edit, preview, and send

Modify mode retains incoming packets for later replay or editing. Select a
decoded received packet, change its editable fields, choose **Preview changes**,
and then choose **Confirm and send** to send the edited copy to the configured
target. Incoming traffic is never held while an operator edits a packet.

![Modify, preview, and send a packet](docs/modify_modify_demo.gif)

#### Change mode or listen address in the UI

The status card can change modes without restarting the application. Select
**Workbench Mode**, choose **Relay** or **Modify**, enter a `host:port` forward
target, and select the activation button. Switching back to Listen removes the
active forward target. Packet history and capture state are preserved.

![Switch from listen to relay mode](docs/switch_to_relay_mode_demo.gif)

The **Listen on** field changes the active GSMTAP UDP listener without a
restart. Enter an IPv4 `address:port` and select **Save**. The address must
exist on a host network interface; the HTTP listener remains configured at
startup.

![Change the listen address](docs/listen_changeAddress_demo.gif)

For mode-by-mode use cases, see
[Mode use cases](docs/mode-use-cases.md).

The service retains only a bounded, oldest-first history (default 10,000
packets), while the UI renders a recent 500-packet page and can later select
one packet to replay or modify. `/api/status` reports receive, parse,
ingress-drop, history-eviction, UI-event-drop and reference-match counters.

The workbench is intended for trusted lab or development traffic. It retains
packet content in memory for inspection, so do not expose it directly to
untrusted high-volume networks without applying deployment-specific bounds and
access controls.

### Local interactive demo

To send continuous sample traffic to an isolated local address, create the
dummy interface below, start the workbench, and run the traffic generator in a
second terminal:

```bash
sudo ip link add gsmtap-demo type dummy
sudo ip addr add 10.200.0.10/24 dev gsmtap-demo
sudo ip link set gsmtap-demo up
python3 tests/traffic/continuous.py
```

To demonstrate Relay or Modify mode with a downstream UDP service on the same
host, assign a second address before starting the demo:

```bash
sudo ip addr add 10.200.0.20/24 dev gsmtap-demo
```

Set **Listen on** to `10.200.0.10:4729` and **Forward to** to
`10.200.0.20:4729`. The downstream application must bind a UDP socket to
`10.200.0.20:4729`; relay sends selected packets there, while modify sends
explicit replays and edited packets there.

The generator sends to `10.200.0.10:4729` by default. Stop it with `Ctrl-C`,
then remove the temporary interface when finished:

```bash
sudo ip link delete gsmtap-demo
```

## Reference harness

The harness and fixtures follow three rules:

1. **Recorded outputs define behavior.** The direct-call C harnesses in
   `reference/` call public libosmocore APIs and record return values, buffer
   contents and callback traces. The fixtures in `tests/vectors/` are the
   behavioral specification.
2. **The C source was consulted** to understand those results and decide what
   carries over to Rust.
3. **osmocore-rs is idiomatic Rust** verified against the fixtures. No C code,
   comments or data tables were copied.

The top-level `tests/vectors/*.json` files are the GSMTAP packet vectors that
the workbench and the traffic-test image load. Each other module has its own
subdirectory, shared byte-for-byte with osmocore-rs's
`tests/compatibility/vectors/`. The C generator output in this repository is
canonical.

| Module | Fixtures | Harness details |
| --- | --- | --- |
| GSMTAP encoder | `tests/vectors/*.json` | [Encoder vectors](reference/README.md#encoder-vectors) |
| BCD | `tests/vectors/bcd/` | [BCD conversion vectors](reference/README.md#bcd-conversion-vectors) |
| Hexparse | `tests/vectors/hexparse/` | [Hexparse buffer vectors](reference/README.md#hexparse-buffer-vectors) |
| Bit packing | `tests/vectors/bits/` | [Basic](reference/README.md#basic-bit-packing-vectors) and [extended](reference/README.md#extended-bit-packing-vectors) bit-packing vectors |
| Base64 | `tests/vectors/base64/` | [Base64 reference](reference/README.md#base64-workbench-reference) |
| Bit vectors | `tests/vectors/bitvec/` | [Bit-vector stages](reference/README.md#bit-vector-primitives-first-stage) |
| Message buffers | `tests/vectors/msgb/` | [Message-buffer stages](reference/README.md#message-buffer-stage-1) |
| Primitives | `tests/vectors/prim/` | [Primitive observations](reference/README.md#primitive-observations) |
| CRC | `tests/vectors/crc/` | [Core CRC observations](reference/README.md#core-crc-observations) |
| Convolutional codes | `tests/vectors/conv/` | [Convolutional evidence](reference/README.md#convolutional-evidence-and-backend-gate) |
| Time and timers | `tests/vectors/time/` | [Time and timer evidence](reference/README.md#time-and-timer-evidence-stages-12) |
| TDef | `tests/vectors/tdef/` | [TDef core evidence](reference/README.md#tdef-core-evidence-stage-3) |
| time_cc | `tests/vectors/time_cc/` | [time_cc evidence](reference/README.md#time_cc-evidence-stage-4) |
| Rate counters | `tests/vectors/rate_ctr/`, `tests/vectors/rate_ctr_group/` | [Standalone counters](reference/README.md#standalone-rate-counter-evidence-stage-5a) and [counter groups](reference/README.md#stage-7-rate-counter-groups-and-rollups) |
| FSM | `tests/vectors/fsm_*/` | [FSM execution and lifecycle](reference/README.md#fsm-execution-lifecycle-and-tdef-integration-stage-6) |
| Networking | `tests/vectors/net/` | [Networking reference](reference/network/README.md) |

The runners build against the libosmocore checkout at
`../libosmocore-rs/libosmocore` and fail rather than fall back to a system
installation. To regenerate every fixture:

```bash
./scripts/generate-reference-vectors.sh
git status tests/vectors
```

A clean `git status` means the C output still matches the committed fixtures.
Review any diff before accepting it. [reference/README.md](reference/README.md)
covers provenance, pinned source hashes and per-module regeneration commands.

## Continuous integration and releases

One GitHub Actions workflow, `.github/workflows/ci.yml`, runs three jobs on
pushes to `main` and on pull requests:

- `ui`: JavaScript syntax checks and the frontend tests.
- `rust`: `cargo fmt`, `cargo clippy`, `cargo test` and `cargo deny check`,
  which covers licenses, sources and RustSec advisories.
- `container`: `scripts/integration.sh`, which builds both images and runs the
  listen, relay and modify traffic checks.

Pushing a `vX.Y.Z` tag, such as `v0.1.1`, also runs a `release` job once those
checks pass. It checks the tag against the crate version, writes a CycloneDX
SBOM, packages the crate, and builds the workbench image and scans it with
Trivy. It then publishes the crate to crates.io, pushes
`ghcr.io/silent-wolf-labs/gsmtap-rs:X.Y.Z` and `:latest`, and creates a GitHub
release with the crate, SBOM, scan report and SHA256 checksums. Tags with a
suffix, such as `v1.0.0-rc1`, do not release.

### Traffic checks

The `traffic-test` image replays every committed `tests/vectors/*.json`
`encoded_hex` value as opaque bytes generated by the libosmocore reference
harness. It does not implement GSMTAP encoding. Run the same checks as CI with:

```bash
./scripts/integration.sh
./scripts/integration.sh relay   # one mode only
```

The script builds the workbench and traffic-test images once, creates a private
Podman network, and runs each mode against a fresh workbench. The traffic
container is the test assertion: it confirms receive counts through the
status/API, checks explicitly batch-forwarded relay datagrams byte-for-byte,
including a malformed datagram, monitors the listen sink for unexpected
forwarding, and validates modify preview/replay/send effects. With
`--require-references`, which the script passes, it also requires every
replayed vector to come back tagged with its recorded case.

For direct local checks, use the mode-specific scripts. For example, start the
workbench, select Relay in the status card, set its forward target to
`127.0.0.1:9000`, then run:

```bash
python3 tests/traffic/relay.py \
  --workbench-host 127.0.0.1 \
  --workbench-port 8080 \
  --listen-port 4729 \
  --sink-port 9000 \
  --vectors tests/vectors
```

`tests/traffic/listen.py` and `tests/traffic/modify.py` provide equivalent
checks for those modes. `replay.py --mode …` is the entry point the traffic-test
image uses.

## License

This project is licensed under the Apache License, Version 2.0. See
[LICENSE](LICENSE) for the full text.
