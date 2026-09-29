# libosmocore reference harness

This directory contains small C programs that generate conformance inputs by
calling `libosmocore`. They are the source of truth for expected behavior:
Rust code must not participate in generating their output.

## Encoder vectors

`gsmtap_makemsg_ex_vector.c` selects a named input from a small table and calls
either `gsmtap_makemsg_ex()` or `gsmtap_makemsg()`. It writes a JSON observation
containing the complete resulting message as lowercase hexadecimal. It targets
`libosmocore` revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

The `case` field is a stable fixture-contract identifier. Renaming, removing,
or materially changing a case requires explicit fixture-contract review.

Run it against a configured `libosmocore` installation:

```bash
source ./scripts/libosmocore-env.sh
./scripts/generate-reference-vectors.sh
```

The generator intentionally captures only C-produced output in
`tests/vectors/`. Rust code must only consume those fixtures.

Review every regeneration diff, including metadata and encoded bytes. A
reference-commit change with identical bytes is informative. Any changed bytes
require an explicit explanation before accepting the regenerated fixture.

## BCD conversion vectors

`bcd_conversion_vector.c` calls `osmo_bcd2char()`, `osmo_char2bcd()`, `osmo_bcd2str()`, and
`osmo_str2bcd()` directly. It records source bytes, offsets, flags, initial and
final destination bytes, and C return values as JSON. The runner checks hashes
of `utils.c`, `utils.h`, and `utils_test.c` against the local files previously
verified for revision `950430e829a3dc1d162aa241bc0505745c5a7311`.

`scripts/generate-reference-vectors.sh` writes the 76 BCD observations as one
JSON array in `tests/vectors/bcd/bcd_conversion_vectors.json`. The BCD-specific
directory keeps conversion records out of GSMTAP packet and traffic replay
loaders. The `gsmtap-rs` test checks fixture structure and provenance; the
`libosmocore-rs` test compares Rust BCD behavior with the C observations.

From the `libosmocore-rs` repository root, regenerate the fixture with:

```bash
../gsmtap-rs/reference/run_bcd_conversion_vector.sh > libosmocore-rs/tests/vectors/bcd_conversion_vectors.json
```

The two committed BCD fixtures should match byte for byte. To regenerate only
the `gsmtap-rs` copy, run from this repository root:

```bash
./reference/run_bcd_conversion_vector.sh > tests/vectors/bcd/bcd_conversion_vectors.json
```

To verify the committed `gsmtap-rs` fixture against a fresh C run without
replacing it, run:

```bash
./reference/run_bcd_conversion_vector.sh | cmp - tests/vectors/bcd/bcd_conversion_vectors.json
```

The cases use only in-bounds C arrays. Review the complete fixture diff after
regeneration; ordinary Rust tests use the committed JSON without running C.
