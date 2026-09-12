# gsmtap-rs

`gsmtap-rs` is a Rust implementation of GSMTAP packet parsing and encoding.
It also provides a focused testbed for validating conversions of relevant
`libosmocore` C and C++ code into Rust.

The goal is behavioral compatibility where it matters: each converted feature
should have Rust tests that document and verify its packet layout, parsing
behavior, and error handling against the reference implementation.

## Repository Structure

- `src/` — library source code
- `tests/` — integration and compatibility tests
- `../libosmocore/` — read-only reference checkout used for research and
  behavior comparison; it is not a Cargo dependency

## Building

This project requires Rust 1.85 or newer. To build it, run:

```bash
cargo build
```

## Testing

To run tests, execute:

```bash
cargo test
```

## Live web workbench

Build and run the single-process workbench with:

```bash
cargo run --bin gsmtap-workbench -- \
  --gsmtap-listen 0.0.0.0:4729 \
  --mode listen \
  --http-listen 0.0.0.0:8080
```

Open `http://localhost:8080`. The equivalent environment variables are
`GSMTAP_MODE`, `GSMTAP_LISTEN`, `GSMTAP_FORWARD`, and `HTTP_LISTEN`;
command-line values take precedence. `PACKET_HISTORY_CAPACITY` bounds
in-memory RX/TX history (default 10,000 packets and a fixed 256 MiB ceiling for
retained record data), while
`PACKET_INGRESS_CAPACITY` independently bounds unprocessed incoming datagrams
(default 1,024). The API and browser render only a recent page:
`/api/packets` defaults to 500 and caps requests at 1,000.

The 256 MiB history-data ceiling is intentionally fixed in this release and is
separate from the packet-count setting. If a deployment needs a different
memory limit, the ceiling should be made configurable through a dedicated
environment variable in a future change.

The native workbench binds its HTTP interface to `127.0.0.1:8080` by default;
set `HTTP_LISTEN` or `--http-listen` deliberately when remote access is
required. The current UDP listener and forwarding path are IPv4-only. Hostname
forward targets are supported; the hostname is resolved for each explicit send
operation and only an IPv4 result is used.

The container image exposes TCP port 8080 and UDP port 4729:

```bash
docker build -t gsmtap-workbench .
docker run --rm -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench --mode listen
```

The browser only calls the application API. GSMTAP parsing and encoding remain
implemented by the Rust library.

### Operating modes

The three operating modes are shown below. The web UI is available in every
mode: listen and relay provide inspection, while modify additionally provides
explicit edit, preview, and send controls.

![GSMTAP workbench listen, relay, and modify modes](docs/mode_use_cases.svg)

For the mode-by-mode use cases and Docker examples, see
[Mode use cases](docs/mode-use-cases.md).

Use `--mode listen` for passive inspection. Use `--mode relay` with
`--gsmtap-forward HOST:PORT` to record incoming datagrams for inspection and
forward explicitly selected batches from the UI. Forwarded relay packets retain
their original bytes; the downstream peer will see the workbench as the UDP
source. Use `--mode modify` with the same forward option to hold packets for
explicit replay or field editing from the UI.

Modify mode is capture-and-replay, not an inline human-held proxy: incoming
packets are never paused waiting for an edit. The service retains only a
bounded, oldest-first history (default 10,000 packets), while the UI renders a
recent 500-packet page and can later select one packet to replay or modify.
`/api/status` reports receive, parse,
ingress-drop, history-eviction, and UI-event-drop counters.

The workbench is intended for trusted lab or development traffic. It retains
packet content in memory for inspection, so do not expose it directly to
untrusted high-volume networks without applying deployment-specific bounds and
access controls.

Relay example:

```bash
docker run --rm -p 8080:8080 -p 4729:4729/udp gsmtap-workbench \
  --mode relay --gsmtap-forward 192.168.1.50:4729
```

Modify mode uses the UI to explicitly replay or edit a packet:

```bash
docker run --rm -p 8080:8080 -p 4729:4729/udp gsmtap-workbench \
  --mode modify --gsmtap-forward 192.168.1.50:4729
```

For a local container smoke check, start the image in the intended mode and
open `http://localhost:8080`. Listen mode must not forward received packets;
relay mode records received packets and forwards only the packets selected with
the UI's Forward selected action; modify mode requires using the UI's Preview
changes and Confirm and send controls. GitHub Actions runs
formatting, linting, tests, a release build, security checks, and Docker
integration checks on pushes and pull requests. Releases are created from
matching semantic-version tags such as `v0.1.0`: the crate is published to
crates.io, and the supported workbench image is published to
`ghcr.io/bucketking657/gsmtap-rs:0.1.0`. Stable releases also update the
`latest` tag; prereleases do not. GitHub Releases include the CycloneDX SBOM
and SHA256 checksums. CI also builds both container targets and runs isolated
listen, relay, and modify traffic checks.

### Reference-vector traffic checks

Build the slim Python traffic image with:

```bash
docker build --target traffic-test -t gsmtap-traffic-test .
```

It replays every committed `tests/vectors/*.json` `encoded_hex` value as
opaque bytes generated by the libosmocore reference harness. It does not
implement GSMTAP encoding. CI checks listen, byte-preserving relay (including
a malformed datagram), and modify-mode preview, replay, and explicit send.
The same checks can be run locally with Docker Compose:

```bash
GSMTAP_MODE=listen docker compose -f tests/integration/compose.yaml up --build --abort-on-container-exit --exit-code-from traffic
GSMTAP_MODE=relay docker compose -f tests/integration/compose.yaml up --build --abort-on-container-exit --exit-code-from traffic
GSMTAP_MODE=modify docker compose -f tests/integration/compose.yaml up --build --abort-on-container-exit --exit-code-from traffic
docker compose -f tests/integration/compose.yaml down --remove-orphans --volumes
```

Each invocation is an isolated Compose project with one workbench and one
traffic verifier service.
The verifier is the test assertion: it confirms receive counts through the
status/API, checks explicitly batch-forwarded relay datagrams byte-for-byte,
monitors the listen sink for unexpected forwarding, and validates modify
preview/replay/send effects.

For direct local checks, use the mode-specific scripts. For example, after
starting the workbench in relay mode with
`--gsmtap-forward 127.0.0.1:9000`, run:

```bash
python3 tests/traffic/relay.py \
  --workbench-host 127.0.0.1 \
  --workbench-port 8080 \
  --listen-port 4729 \
  --sink-port 9000 \
  --vectors tests/vectors
```

`tests/traffic/listen.py` and `tests/traffic/modify.py` provide equivalent
checks for those modes. `replay.py --mode …` remains available for Compose and
automation compatibility.

## Reference implementation

The reference implementation is located at `../libosmocore`. Treat it as
read-only: it is useful for identifying the intended C/C++ behavior, but it is
not a build-time or runtime dependency of this crate.

## Conversion workflow

1. Identify the `libosmocore` function or packet format to port.
2. Capture its expected behavior in Rust tests, including malformed input and
   boundary cases.
3. Implement the Rust equivalent in `src/`.
4. Run `cargo test` and record any intentional behavior differences.

## License

This project is licensed under the Apache License, Version 2.0. See
[LICENSE](LICENSE) for the full text.
