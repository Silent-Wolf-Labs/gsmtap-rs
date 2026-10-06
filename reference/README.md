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

All direct-call runners build and link against the canonical C checkout at
`../libosmocore-rs/libosmocore`, relative to this repository. This checkout is
intentionally ignored by Git in `libosmocore-rs`. Do not substitute another
checkout or a system-installed C library. Missing reference files or a failed
build stop generation; there is no fallback. The obsolete
`/mnt/storage/git/libosmocore` location is no longer used.

From this repository root, regenerate the fixtures with:

```bash
./scripts/generate-reference-vectors.sh
```

The runner paths resolve from their script locations and also work when invoked
from another working directory. Generation does not require `pkg-config` or an
installed libosmocore: the runners use this checkout's headers and freshly built
core library, with a runtime library search path pointing to that build.

For other tools that need an installed library, source
`scripts/libosmocore-env.sh` in Bash. It selects this checkout's `_install/`
directory, checks `pkg-config` provenance, and rejects `LIBOSMOCORE_ROOT` values
pointing elsewhere. A missing local installation is an error. For example:

```bash
source ./scripts/libosmocore-env.sh
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

## Hexparse buffer vectors

`hexparse_vector.c` calls `osmo_hexparse()` directly and records each C-string
input including its NUL terminator, the initialized destination, capacity,
return code, and the complete final destination as JSON. It targets upstream
libosmocore revision `950430e829a3dc1d162aa241bc0505745c5a7311`. The
local `utils.c`, `utils.h`, and `utils_test.c` files were compared byte-for-byte
with that GitHub revision. The runner checks the hashes of `utils.c` and
`utils.h` and rebuilds the local core library before executing the harness.

`scripts/generate-reference-vectors.sh` writes the 21 observations to
`tests/vectors/hexparse/hexparse_buffer_vectors.json`. The `gsmtap-rs` test
checks fixture structure and provenance; the `libosmocore-rs` test compares
Rust behavior with the C observations. Both committed fixtures should match
byte for byte.

From the `libosmocore-rs` repository root, regenerate the Rust fixture with:

```bash
../gsmtap-rs/reference/run_hexparse_vector.sh > libosmocore-rs/tests/vectors/hexparse_buffer_vectors.json
```

Review the generated diff before committing. The normal Rust tests consume the
committed fixture and do not need the C library.

To regenerate only the `gsmtap-rs` copy, run from this repository root:

```bash
./reference/run_hexparse_vector.sh > tests/vectors/hexparse/hexparse_buffer_vectors.json
```

To verify the committed fixture against a fresh C run without replacing it:

```bash
./reference/run_hexparse_vector.sh | cmp - tests/vectors/hexparse/hexparse_buffer_vectors.json
```

## Basic bit-packing vectors

`bit_packing_vector.c` directly calls `osmo_ubit2pbit()` and
`osmo_pbit2ubit()`. This scope covers basic MSB-first packing and unpacking.
Extended variants, soft-bit conversions, and CRC helpers are separate future
conversions, each requiring its own harness support. No bit-conversion API is
added to the GSMTAP library, workbench, or replay tools.

The reference is upstream libosmocore revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. On 2026-10-05, the following
local files were compared byte-for-byte with files downloaded from the
[upstream pinned revision](https://github.com/osmocom/libosmocore/tree/950430e829a3dc1d162aa241bc0505745c5a7311):

| File | Verified SHA-256 |
| --- | --- |
| `src/core/bits.c` | `bb33ceafed9bc49da6c002804108cdcd23914f6f04921b0c95645cd1ddc9fa1d` |
| `include/osmocom/core/bits.h` | `922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7` |

`run_bit_packing_vector.sh` checks these hashes, rebuilds the canonical
checkout's `src/core`, and uses its headers and `.libs/libosmocore` for linking
and execution. It overrides inherited `LD_LIBRARY_PATH` and `LD_PRELOAD` for
harness execution. Missing files, mismatched hashes, build or execution failures
stop generation; temporary executables are removed on exit. Verification used
individual upstream files in temporary storage, without creating another C
checkout. The reference directory has no `.git`; its parent Rust repository's
Git revision is not C provenance.

The dedicated fixture is `tests/vectors/bits/bit_packing_vectors.json`, a JSON
array of **9,016** observations in fixed order:

- 8,192 exhaustive cases: bytes `00` through `ff`, lengths 1 through 8,
  unpacking and packing of the corresponding eight binary input bytes, each
  with exact and oversized destinations. This includes partial-byte packing.
- 780 boundary cases: lengths 7, 8, 9, 15, 16, 17, 31, 32, and 33, using
  all-zero, all-one, alternating (first bit set), and every single-set-bit
  position. Both APIs use exact and oversized destinations.
- 36 extra-source cases: at each boundary length, both APIs receive identical
  requested input with contrasting three-byte source suffixes. Packing suffixes
  remain binary; unpacking suffixes are `00` or `ff`. Destinations are oversized.
- 8 zero-bit cases: both APIs, source first bit zero or one, and one-byte or
  four-byte backing destinations.

Exhaustive cases come first, ordered by byte, bit count, API (unpack then pack),
then destination (exact then oversized). For each boundary length, patterns
come in zero, one, alternating, then single-bit position order; each pattern
uses the same API and destination order. Extra-source cases follow that length's
patterns, ordered by API and suffix (zero then one). Zero-bit cases come last,
ordered by API, source bit, then backing destination size. Case names, inputs,
order, and count are frozen in the fixture validator; changes require
fixture-contract review.

Every record has exactly `case`, `api`, `libosmocore_commit`, `num_bits`,
`src_hex`, `dst_len`, `dst_before_hex`, `return_code`, and `dst_after_hex`.
Hexadecimal is lowercase. Sources are recorded in full; source and destination
backing arrays are allocated at exactly the recorded lengths. Destinations are
initialized with `0xa5 ^ index`.
Oversized destinations add three bytes. Return values and final buffers come
only from the direct C calls, never from input construction or Rust.

For positive bit counts, packing returns the packed-byte count, clears unused
low bits in a partial final byte, and preserves the destination suffix.
Unpacking writes binary bytes, returns the bit count, and preserves its suffix.
Direct observations also retain the zero-bit edge behavior: packing returns
`0` with no writes; unpacking reads `src[0]`, writes its top bit to `dst[0]`,
and returns `1`. Every zero-bit C call has nonempty source and destination
backing arrays. These results must not be normalized to a future Rust contract.
Undersized C buffers and NULL pointers are unsafe and are never harness cases;
nonbinary unpacked input is outside this fixture's domain.

From this repository root, regenerate all reference fixtures:

```bash
./scripts/generate-reference-vectors.sh
```

To regenerate only this fixture, or compare it with fresh C output:

```bash
./reference/run_bit_packing_vector.sh > tests/vectors/bits/bit_packing_vectors.json
./reference/run_bit_packing_vector.sh > /tmp/bit_packing_vectors.fresh.json
cmp /tmp/bit_packing_vectors.fresh.json tests/vectors/bits/bit_packing_vectors.json
cargo fmt --check
cargo test
git diff --check
```

Review all regeneration diffs, including GSMTAP, BCD, and hexparse fixtures.
The bits subdirectory is excluded by the existing nonrecursive packet/replay
loaders. `tests/bit_packing_reference_vectors.rs` consumes the committed JSON
without building C. It validates the exact schema, provenance, frozen input
manifest and order, safe backing sizes, return values, complete destinations,
partial-byte clearing, untouched suffixes, and capacity/source-suffix
independence. It also checks rejection of corrupt observations and normalized
zero-bit results. It does not implement Rust conversion APIs.

The coordinated Rust port belongs in the sibling repository's nested crate.
Its conformance test should compare API return counts and complete destinations
against the same C fixture. From that repository root, after agreeing on the
Rust API and deliberate safety differences:

```bash
../gsmtap-rs/reference/run_bit_packing_vector.sh > libosmocore-rs/tests/vectors/bit_packing_vectors.json
cmp ../gsmtap-rs/tests/vectors/bits/bit_packing_vectors.json libosmocore-rs/tests/vectors/bit_packing_vectors.json
cargo fmt --manifest-path libosmocore-rs/Cargo.toml --check
cargo test --manifest-path libosmocore-rs/Cargo.toml
```

That fixture and Rust port are coordinated follow-up work, not deliverables
implemented by this harness-only change. Safe Rust zero-work behavior for
zero bits, empty or undersized slices, invalid input, and arithmetic limits
must be documented and tested separately from compatible C observations.
Testing support completion alone does not complete the Rust conversion.
