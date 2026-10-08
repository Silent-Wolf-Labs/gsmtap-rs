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
../gsmtap-rs/reference/run_bcd_conversion_vector.sh > libosmocore-rs/tests/compatibility/vectors/bcd_conversion_vectors.json
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
../gsmtap-rs/reference/run_hexparse_vector.sh > libosmocore-rs/tests/compatibility/vectors/hexparse_buffer_vectors.json
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
Extended variants have their own harness below. Soft-bit conversions and CRC
helpers remain separate future conversions requiring their own harness support.
No bit-conversion API is added to the GSMTAP library, workbench, or replay tools.

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
../gsmtap-rs/reference/run_bit_packing_vector.sh > libosmocore-rs/tests/compatibility/vectors/bit_packing_vectors.json
cmp ../gsmtap-rs/tests/vectors/bits/bit_packing_vectors.json libosmocore-rs/tests/compatibility/vectors/bit_packing_vectors.json
cargo fmt --manifest-path libosmocore-rs/Cargo.toml --check
cargo test --manifest-path libosmocore-rs/Cargo.toml
```

That fixture and Rust port are coordinated follow-up work, not deliverables
implemented by this harness-only change. Safe Rust zero-work behavior for
zero bits, empty or undersized slices, invalid input, and arithmetic limits
must be documented and tested separately from compatible C observations.
Testing support completion alone does not complete the Rust conversion.

## Extended bit-packing vectors

`bit_packing_ext_vector.c` calls `osmo_ubit2pbit_ext()` and
`osmo_pbit2ubit_ext()` directly. Expected returns and complete final buffers
come exclusively from those C calls. The fixture is
`tests/vectors/bits/bit_packing_ext_vectors.json`; it stays outside the
nonrecursive packet and replay loaders. No conversion API is added to the
GSMTAP library or tools.

The upstream revision and two source hashes are the same as for basic packing
above. On 2026-10-06, both files were again downloaded individually from that
pinned upstream revision into temporary storage and compared byte-for-byte
with the canonical local reference. The extended runner checks both hashes,
rebuilds `src/core`, links its shared library, and selects that same library at
runtime. It replaces inherited `LD_LIBRARY_PATH` and clears `LD_PRELOAD` and
`LD_AUDIT` for execution. Hash mismatches, missing local references, unsupported
integer ABIs, and build/link/execution failures stop generation. Temporary
executables are removed on exit. No alternate checkout or system installation
is used.

The supported and recorded C ABI has 8-bit bytes, **32-bit unsigned int**, and
32-bit signed int (`INT_MAX == 2147483647`). Each record has exactly these
fields, in this emission order:

```text
case, api, libosmocore_commit, c_unsigned_bits, out_ofs, in_ofs,
num_bits, lsb_mode, src_hex, dst_len, dst_before_hex, return_code,
dst_after_hex
```

Names, API and revision are strings. Width, offsets, counts and destination
lengths are nonnegative integers. `lsb_mode` and `return_code` are signed C int
values represented as JSON integers, never floats. Hex strings contain complete
lowercase bytes. Sources and before/after destinations are recorded in full;
C allocations use exactly those recorded lengths. Positive calls have sufficient
backing and nonoverflowing offset arithmetic and representable return counts.
Zero-count calls still use real one-byte backing, even for offsets beyond it.
Unsafe undersized C calls are excluded.

The input manifest and order are frozen in
`tests/bit_packing_ext_reference_vectors.rs`: **63,384 observations**.
The families are emitted in this order:

| Family | Cases | Coverage and nested order |
| --- | ---: | --- |
| `exhaustive` | 36,864 | Byte `00..ff`, offset `0..7`, count `1..8-offset`, unpack then pack, mode `0` then `1` |
| `boundary` | 19,104 | Output offset, equal/next input offset, count, pattern, API, mode |
| `offsets` | 5,760 | All independent output/input offset pairs, count, API, mode |
| `truthy` | 1,296 | Value `02`, `80`, `ff`; output offset; mode `0`, `1`, `2`, `-1`; count `1`, `9`, `17`; initialization |
| `neighbors` | 192 | API, mode `0`, `1`, `2`, `-1`, output offset, zero/all-one source neighbors |
| `zero` | 168 | API, mode `0`, `1`, `2`, `-1`, output offset `0`, `1`, `7`, `8`, `9`, `16`, `65`; input offset `0`, `9`, `65` |

The common offset list is `0,1,2,3,4,5,6,7,8,9,15,16`; counts are
`1,7,8,9,15,16,17,31,32,33`. Boundary patterns are zero, one, alternating
(first bit set), then each single-set-bit position. The next-offset pairing
wraps from `16` to `0`. API order is unpack (`p0`) then pack (`p1`), except the
packing-only truthy family. Case identifiers include each family's independent
loop parameters; their exact spellings are frozen by the validator.

Exhaustive unpacking uses the byte as source, the chosen offset as input, and
`(byte + count) % 8` as output offset. Exhaustive packing uses the eight source
bits encoded according to the selected mode, a three-byte sentinel prefix,
input offset `3 + offset`, and output offset `offset`. The whole byte is
represented even for partial requests. Exact versus oversized destinations
(additional three bytes) alternate by the sum of loop parameters modulo two;
initialization cycles by the same sum modulo three through `00`, `ff`, and
`a5 ^ byte_index`. The boundary and independent-offset families use the same
capacity/initialization scheduling, based on loop indices. They retain three
source suffix bytes and unused source prefixes.

Truthy cases alternate the selected nonbinary value with zero, with input
offset three and sentinel source neighbors. They use oversized destinations
and all three initializations explicitly. Neighbor cases use input offset nine,
count seventeen, alternating requested input, oversized mixed destinations,
and contrasting zero/all-one bytes and partial-byte bits outside the source
range. Their requested input is identical despite different neighbors. Zero
cases use source `ff` and cycle destination initialization by loop indices.
These rules construct inputs only; they never supply expected converted output.

Packing treats every nonzero source byte as one and sets or clears only selected
destination bits. Zero mode orders bits MSB first; **any nonzero integer mode**
orders them LSB first. Bit order uses the output bit position for packing and
the input bit position for unpacking. Packed offsets count bits; unpacked
offsets count byte-sized bits. Unpacking writes only zero or one and preserves
all destination bytes outside its selected range.

For positive counts, packing returns `ceil((out_ofs + num_bits) / 8)`;
unpacking returns `out_ofs + num_bits`. These are ending output positions,
not counts newly written. Both extended APIs perform no writes or source reads
for zero counts. Unpacking then returns `out_ofs`. Packing with positive output
offset returns `ceil(out_ofs / 8)`; at output offset zero its unsigned subtraction
underflows, producing **536870912** on the recorded ABI. This return can exceed
backing capacity and must not be normalized to zero. This behavior differs from
the basic unpacking API's zero-count read/write.

Regenerate or verify from the `gsmtap-rs` root:

```bash
./scripts/generate-reference-vectors.sh
./reference/run_bit_packing_ext_vector.sh > /tmp/bit_packing_ext.fresh.json
cmp /tmp/bit_packing_ext.fresh.json tests/vectors/bits/bit_packing_ext_vectors.json
cargo fmt --check
cargo test
git diff --check
```

Review every fixture diff from full regeneration. The basic bit-packing fixture
and all other existing observations must remain unchanged. The existing GSMTAP
generator emits compact JSON while the committed packet fixtures are formatted;
review those formatting-only diffs and preserve their committed formatting.
Ordinary Rust tests
consume committed JSON and need no C build. The extended validator checks the
exact schema, frozen names/order/count/inputs, source provenance and ABI, scalar
ranges, safe backing, complete destinations, ending-position returns, untouched
partial-byte bits/prefixes/suffixes, and binary unpacked output. It compares C
observations for source-neighbor, truthy-value, and nonzero-mode independence.
Its corruption checks reject malformed records, changed zero-count returns,
normalized underflow results, and changed untouched storage bits in both modes.
It does not reimplement the extended conversion to generate expected outputs.

The separate `libosmocore-rs` task owns the Rust APIs, safe-slice and arithmetic
error tests, and comparison of full Rust returns/destinations to this fixture.
Its owner should copy this completed JSON unchanged into the nested crate's
`tests/vectors/bit_packing_ext_vectors.json` and compare it byte-for-byte.
This task changes only `gsmtap-rs` source, fixtures, documentation and Git state;
it does not copy files into or implement APIs in the sibling repository.
Testing support completion is one part of the overall conversion.

## Base64 workbench reference

The Base64 endpoint uses the existing sibling Rust APIs. Its 24 direct C
observations are shared byte-for-byte with the sibling crate's
`tests/vectors/base64_buffer_vectors.json`; this repository keeps them under
`tests/vectors/base64/`, outside packet loaders. The original harness initializes
all destination backing bytes to `0xa5` and the length to `0xdecafbad`. It emits
complete final logical destinations, including untouched suffixes. Endpoint
tests assert the full initialized and final buffers and the preserved length
sentinel in Listen, Relay, and Modify modes.

`run_base64_buffer_vector.sh` now supports full-array regeneration (no arguments),
`--list`, and the existing `--case NAME` interface. It pins SHA-256 hashes for
`src/core/base64.c`, `include/osmocom/core/base64.h`, and
`tests/base64/base64_test.c`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. It builds the canonical local C
reference, links its explicit shared library, and selects that directory at
runtime with preload cleared, failing if the reference is unavailable.

```bash
./reference/run_base64_buffer_vector.sh > /tmp/base64.fresh.json
cmp /tmp/base64.fresh.json tests/vectors/base64/base64_buffer_vectors.json
cmp tests/vectors/base64/base64_buffer_vectors.json ../libosmocore-rs/libosmocore-rs/tests/compatibility/vectors/base64_buffer_vectors.json
./scripts/generate-reference-vectors.sh
```

Review all regeneration diffs and preserve formatting of existing GSMTAP
fixtures after confirming their parsed values remain identical. Ordinary
endpoint tests read the committed observations without building C.

## Bit-vector primitives (first stage)

`bitvec_vector.c` makes 5,944 direct calls covering positional binary/L/H reads,
all four bit values for positional and sequential writes, sequential L/H reads,
bulk writes (including partial failures), zeroing, used-byte/tailroom helpers,
and bit-value characters. Every call records returns, cursor state, and all
four backing bytes, including the suffix beyond the declared vector length.
Inputs span lengths zero to three, three byte patterns, every in-range position,
byte boundaries, and invalid positions/cursors including `UINT_MAX`.

`run_bitvec_vector.sh` checks the source, header, and upstream test SHA-256 hashes,
verified byte-for-byte against revision `950430e829a3dc1d162aa241bc0505745c5a7311`.
It builds and links only the required sibling `libosmocore` reference, clears
runtime library overrides, and enforces a 32-bit C unsigned-int ABI. It never
uses a substitute C installation.

The dedicated `tests/vectors/bitvec/bitvec_vectors.json` fixture stays outside
packet/replay loaders. `tests/bitvec_reference_vectors.rs` validates its schema,
provenance, frozen inputs/order/count, output types, and untouched backing suffix.
Its `tests/support/bitvec_fixture.rs` validator must match the conversion
repository's `libosmocore-rs/tests/core/bitvec_fixture.rs` byte-for-byte.
Rust-versus-C comparisons belong in the sibling crate. Rust preserves the reference's
32-bit unsigned tailroom wraparound directly; a separate checked helper validates
capacity. Invalid enum values are excluded by its type.

From this repository root, verify fresh observations without changing fixtures:

```bash
./reference/run_bitvec_vector.sh > /tmp/bitvec.fresh.json
cmp /tmp/bitvec.fresh.json tests/vectors/bitvec/bitvec_vectors.json
cmp /tmp/bitvec.fresh.json ../libosmocore-rs/libosmocore-rs/tests/compatibility/vectors/bitvec_vectors.json
cmp tests/support/bitvec_fixture.rs ../libosmocore-rs/libosmocore-rs/tests/core/bitvec_fixture.rs
```

For intentional regeneration, review the C-generated diff before copying to both
fixtures. The general generation script includes this runner. This stage adds
harness and validation support; the integer/buffer and indexed-operation stages
are documented below. The workbench's vendored crate is updated only from a
reviewed committed revision, per the agent guide.

## Bit-vector integer and buffer operations (second stage)

`bitvec_buffers_vector.c` and `run_bitvec_vector.sh --buffers` directly observe
`bitvec_set_u64`, `bitvec_set_uint`, `bitvec_get_uint`, `bitvec_get_int16_msb`,
`bitvec_fill`, `bitvec_spare_padding`, `bitvec_get_bytes`, and `bitvec_set_bytes`.
The fixture contains 38,837 calls across lengths zero, one, two, four, and eight,
three initial patterns, boundary/invalid cursors, integer widths from zero to
64 plus oversized widths, and all eight byte alignments. Each record captures
return codes, the cursor, all eight vector backing bytes and six transfer bytes.
Capacity failures, partial progress, preserved neighbors, signed 32-bit results,
count narrowing, and wrapping fill/padding calculations are included.

The runner verifies the same pinned source/header/test hashes as the primitive
fixture, and additionally enforces the reference's signed 32-bit return ABI.
The input manifest excludes undefined C shifts and scalar reads outside the
declared vector length. Wide reads with zero high bits (or an earlier capacity
error) retain their defined C behavior. Rust handles unsafe slice/shift cases
in separate unit tests; it does not generate expected C outcomes.

`tests/bitvec_buffers_reference_vectors.rs` validates schema, provenance, frozen
inputs/order/count, full buffers and guards in the dedicated
`tests/vectors/bitvec/bitvec_buffers_vectors.json` fixture. Its shared validator
and parser, `tests/support/bitvec_buffers_fixture.rs` and `bitvec_fixture.rs`,
must match the corresponding files in the conversion crate's `tests/core/`.
The general fixture generation script includes both bit-vector runners.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --buffers > /tmp/bitvec_buffers.fresh.json
cmp /tmp/bitvec_buffers.fresh.json tests/vectors/bitvec/bitvec_buffers_vectors.json
cmp /tmp/bitvec_buffers.fresh.json ../libosmocore-rs/libosmocore-rs/tests/compatibility/vectors/bitvec_buffers_vectors.json
cmp tests/support/bitvec_buffers_fixture.rs ../libosmocore-rs/libosmocore-rs/tests/core/bitvec_buffers_fixture.rs
```

Review regenerated observations before updating both fixture copies. Normal
Rust tests use committed fixtures and require no C installation.

## Bit-vector indexed operations and helpers (third stage)

`bitvec_operations_vector.c`, selected by `run_bitvec_vector.sh --operations`,
records 38,643 direct calls to indexed field reads/writes, Nth-set-bit and binary
searches, both run-length helpers, left shifts and CSN.1 array encoding/estimates.
The same pinned local source/header/test hashes and build are used; the harness
additionally verifies the signed-int conversion and recorded EINTR/EOVERFLOW ABI.

Cases record all eight backing bytes, scalar 64-bit results, errno, return codes,
and independent external index/cursor states. Coverage includes zero-width field
reads, capacity errors, partial field writes, byte-count narrowing with wrapped
index updates, search sentinels, all valid run cursors, partial limits, invalid
cursors with defined returns, and oversized limits with safe early mismatches.
Shifts preserve the local C routine's retained tail and skipped penultimate byte;
array cases cover dry-run wraparound, ignored setter errors, and partial writes.

The dedicated `tests/vectors/bitvec/bitvec_operations_vectors.json` file stays
outside packet/replay loading. `tests/bitvec_operations_reference_vectors.rs`
checks strict schema/provenance and the frozen input manifest through
`tests/support/bitvec_operations_fixture.rs`. The shared validator and its
`bitvec_fixture.rs` parser must match the conversion crate's `tests/core/` copies.
C input filters avoid undefined storage accesses and shifts; unsafe-call handling
belongs in Rust safety tests. C field-read errno is represented by explicit Rust
errors; Rust does not reproduce the global errno side effect.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --operations > /tmp/bitvec_operations.fresh.json
cmp /tmp/bitvec_operations.fresh.json tests/vectors/bitvec/bitvec_operations_vectors.json
cmp /tmp/bitvec_operations.fresh.json ../libosmocore-rs/libosmocore-rs/tests/compatibility/vectors/bitvec_operations_vectors.json
cmp tests/support/bitvec_operations_fixture.rs ../libosmocore-rs/libosmocore-rs/tests/core/bitvec_operations_fixture.rs
```

The general generation script includes all four stages. Review regenerated
observations before updating both copies. The final buffer/text adapter stage is documented below. Rust storage is
caller-owned instead of allocated through C talloc contexts.

## Bit-vector buffer and text adapters (final stage)

`bitvec_adapters_vector.c`, selected by `run_bitvec_vector.sh --adapters`, records
6,757 calls to full-buffer pack/unpack, hex loading and string formatting. The
runner pins both bitvec and hex-parser source/header/upstream-test SHA-256 hashes,
verified byte-for-byte against revision `950430e829a3dc1d162aa241bc0505745c5a7311`.
All headers, linked libosmocore code and runtime selection use the required local
reference checkout. Source behavior is unchanged.

Cases cover backing lengths zero to eight, exact/oversized transfer buffers,
arbitrary copy cursors, malformed/odd/embedded-NUL/whitespace/high-byte hex
inputs, every possible single input byte, and every valid formatting cursor
through eight bytes over eight patterns. Each call records complete backing,
source and eighty-byte transfer buffers, return codes and cursor state. Output
guards, unused suffixes, partial hex mutations, leading separators, and NUL
terminators are checked. The formatting harness allocates the actual
`cur_bit + ceil(cur_bit / 8) + 1` bytes, including spaces omitted in C's comment.

The fixture is isolated in `tests/vectors/bitvec/bitvec_adapters_vectors.json`.
`tests/bitvec_adapters_reference_vectors.rs` validates its strict schema and
frozen input manifest through `tests/support/bitvec_adapters_fixture.rs`; that
validator and its shared `bitvec_fixture.rs` parser must match the conversion
crate's `tests/core/` copies. Unsafe short buffers/invalid cursor errors belong
in Rust safety tests, not C observations.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --adapters > /tmp/bitvec_adapters.fresh.json
cmp /tmp/bitvec_adapters.fresh.json tests/vectors/bitvec/bitvec_adapters_vectors.json
cmp /tmp/bitvec_adapters.fresh.json ../libosmocore-rs/libosmocore-rs/tests/compatibility/vectors/bitvec_adapters_vectors.json
cmp tests/support/bitvec_adapters_fixture.rs ../libosmocore-rs/libosmocore-rs/tests/core/bitvec_adapters_fixture.rs
```

The general generation script includes all four stages. Review regenerated
observations before copying to both fixtures. All nonallocation bitvec APIs are
covered; C talloc context allocation/free is replaced by caller-owned Rust
storage. The vendored workbench crate remains pinned to its committed provenance
until a reviewed revision is available for refresh.
