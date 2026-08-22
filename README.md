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

This project uses stable Rust. To build it, run:

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
in-memory RX/TX history.

The container image exposes TCP port 8080 and UDP port 4729:

```bash
docker build -t gsmtap-workbench .
docker run --rm -p 8080:8080 -p 4729:4729/udp \
  gsmtap-workbench --mode listen
```

The browser only calls the application API. GSMTAP parsing and encoding remain
implemented by the Rust library.

### Operating modes

Use `--mode listen` for passive inspection. Use `--mode relay` with
`--gsmtap-forward HOST:PORT` to forward original UDP datagrams byte-for-byte.
Use `--mode modify` with the same forward option to hold packets for explicit
replay or field editing from the UI. Relay forwarding remains payload-
transparent; the downstream peer will see the workbench as the UDP source.

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

This project is licensed under the MIT License.
