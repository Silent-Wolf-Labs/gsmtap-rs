# libosmocore reference harness

This directory contains small C programs that generate conformance inputs by
calling `libosmocore`. They are the source of truth for expected behavior:
Rust code must not participate in generating their output.

## Initial vector

`gsmtap_makemsg_ex_vector.c` calls `gsmtap_makemsg_ex()` once with a fixed UM
packet input and writes a JSON observation containing the complete resulting
message as lowercase hexadecimal. It targets `libosmocore` revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

Run it against a configured `libosmocore` installation:

```bash
source ./scripts/libosmocore-env.sh
./scripts/generate-reference-vectors.sh
```

The generator intentionally captures only C-produced output in
`tests/vectors/`. Rust code must only consume that fixture.
