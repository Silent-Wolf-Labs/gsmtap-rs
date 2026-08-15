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
