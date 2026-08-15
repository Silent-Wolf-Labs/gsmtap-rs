# libosmocore reference harness

This directory contains small C programs that generate conformance inputs by
calling `libosmocore`. They are the source of truth for expected behavior:
Rust code must not participate in generating their output.

## Initial vector

`gsmtap_makemsg_ex_vector.c` calls `gsmtap_makemsg_ex()` once with a fixed UM
packet input and writes the complete resulting message as lowercase hexadecimal
followed by a newline. It targets `libosmocore` revision `950430e829a3`.

Run it against a configured `libosmocore` installation:

```bash
PKG_CONFIG_PATH=/path/to/lib/pkgconfig \
  ./reference/run_gsmtap_makemsg_ex_vector.sh
```

The command intentionally emits only the C-produced vector. Capturing that
output as a committed fixture, and consuming it from a Rust integration test,
is the next step in this issue.
