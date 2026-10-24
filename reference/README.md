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

The GSMTAP harness preserves the committed indented JSON layout so full
regeneration is byte-identical as well as behaviorally identical. The existing
`gsmtap_um_uplink_pcs_boundary.json` closing-brace whitespace was normalized
to that layout during prim validation; its values and encoded bytes did not
change.

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
`osmocore-rs` test compares Rust BCD behavior with the C observations.

From the `libosmocore-rs` repository root, regenerate the fixture with:

```bash
../gsmtap-rs/reference/run_bcd_conversion_vector.sh > osmocore-rs/tests/compatibility/vectors/bcd_conversion_vectors.json
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
checks fixture structure and provenance; the `osmocore-rs` test compares
Rust behavior with the C observations. Both committed fixtures should match
byte for byte.

From the `libosmocore-rs` repository root, regenerate the Rust fixture with:

```bash
../gsmtap-rs/reference/run_hexparse_vector.sh > osmocore-rs/tests/compatibility/vectors/hexparse_buffer_vectors.json
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
../gsmtap-rs/reference/run_bit_packing_vector.sh > osmocore-rs/tests/compatibility/vectors/bit_packing_vectors.json
cmp ../gsmtap-rs/tests/vectors/bits/bit_packing_vectors.json osmocore-rs/tests/compatibility/vectors/bit_packing_vectors.json
cargo fmt --manifest-path osmocore-rs/Cargo.toml --check
cargo test --manifest-path osmocore-rs/Cargo.toml
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

The separate `osmocore-rs` task owns the Rust APIs, safe-slice and arithmetic
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
cmp tests/vectors/base64/base64_buffer_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/base64_buffer_vectors.json
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
repository's `osmocore-rs/tests/support/bitvec_fixture.rs` byte-for-byte.
Rust-versus-C comparisons belong in the sibling crate. Rust preserves the reference's
32-bit unsigned tailroom wraparound directly; a separate checked helper validates
capacity. Invalid enum values are excluded by its type.

From this repository root, verify fresh observations without changing fixtures:

```bash
./reference/run_bitvec_vector.sh > /tmp/bitvec.fresh.json
cmp /tmp/bitvec.fresh.json tests/vectors/bitvec/bitvec_vectors.json
cmp /tmp/bitvec.fresh.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/bitvec_vectors.json
cmp tests/support/bitvec_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/bitvec_fixture.rs
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
must match the corresponding files in the conversion crate's `tests/support/`.
The general fixture generation script includes both bit-vector runners.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --buffers > /tmp/bitvec_buffers.fresh.json
cmp /tmp/bitvec_buffers.fresh.json tests/vectors/bitvec/bitvec_buffers_vectors.json
cmp /tmp/bitvec_buffers.fresh.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/bitvec_buffers_vectors.json
cmp tests/support/bitvec_buffers_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/bitvec_buffers_fixture.rs
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
`bitvec_fixture.rs` parser must match the conversion crate's `tests/support/` copies.
C input filters avoid undefined storage accesses and shifts; unsafe-call handling
belongs in Rust safety tests. C field-read errno is represented by explicit Rust
errors; Rust does not reproduce the global errno side effect.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --operations > /tmp/bitvec_operations.fresh.json
cmp /tmp/bitvec_operations.fresh.json tests/vectors/bitvec/bitvec_operations_vectors.json
cmp /tmp/bitvec_operations.fresh.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/bitvec_operations_vectors.json
cmp tests/support/bitvec_operations_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/bitvec_operations_fixture.rs
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
crate's `tests/support/` copies. Unsafe short buffers/invalid cursor errors belong
in Rust safety tests, not C observations.

From this repository root:

```bash
./reference/run_bitvec_vector.sh --adapters > /tmp/bitvec_adapters.fresh.json
cmp /tmp/bitvec_adapters.fresh.json tests/vectors/bitvec/bitvec_adapters_vectors.json
cmp /tmp/bitvec_adapters.fresh.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/bitvec_adapters_vectors.json
cmp tests/support/bitvec_adapters_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/bitvec_adapters_fixture.rs
```

The general generation script includes all four stages. Review regenerated
observations before copying to both fixtures. All nonallocation bitvec APIs are
covered; C talloc context allocation/free is replaced by caller-owned Rust
storage. The vendored workbench crate remains pinned to its committed provenance
until a reviewed revision is available for refresh.

## Message-buffer stage 1

The dedicated `tests/vectors/msgb/msgb_vectors.json` contains 3,695 direct C
operation records, with full backing bytes before/after, data/tail offsets,
length/capacity, layer markers, control words, destination/channel presence,
query results, and normalized panic/error outcomes. The fixture is identical to
`../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/msgb_vectors.json`.
The shared `tests/support/msgb_fixture.rs` validator is identical to the
conversion crate's `tests/support/msgb_fixture.rs`. Tests freeze schema, inputs,
order, provenance, ABI, state consistency, sequence continuity, and corruption
rejection. Normal Cargo tests consume committed JSON without building C.

`reference/msgb_vector.c` calls the actual APIs, including inline primitives.
`reference/run_msgb_vector.sh` pins msgb source/header/upstream tests and panic
source/header, rebuilds the required local core library, and selects it at link
and runtime. All five files were compared byte-for-byte with upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-08. The runner contains
the verified SHA-256 hashes. It fails on missing/mismatched source or unsupported
ABI instead of falling back. The ABI is 8-bit bytes, 32-bit unsigned int,
64-bit unsigned long, and 64-bit pointers.

Coverage includes all offsets/lengths for capacities 0, 1, and 2; selected
zero/half/full offsets and lengths for capacities 8 and 16; zero/exact/excess
arguments; nonzero headroom; trim extension/shrinkage; guarded panic cases;
all optional-layer combinations; reset preservation; reuse sequences; and
65,535-byte capacity. Every before/after state directly queries data, length,
headlen, headroom, tailroom, and invariant. The operation-record count excludes
setup calls and these queries. Head is always zero; pointers are offsets or
null (`-1`). Destination/channel fields record presence, never addresses.
A controlled panic handler uses setjmp/longjmp as in upstream tests.
Void/allocation/invariant observations use return sentinel `-1`; `panic`
distinguishes aborts. Allocation snapshots record actual C zero initialization.

No unsafe C trim with headroom, nonempty reserve, invalid offsets, or NULL
access is used to generate evidence. Rust-only safety tests live in the
conversion repository. That crate owns fixed-capacity storage up to 65,535,
returns checked errors and pointer offsets, and preserves C's wrapping headlen
and reset behavior (L1 and bytes survive; L2–L4 and control words clear).
It checks trim against remaining storage after data and reserve against an
empty message. Fixed `u64` control words match the recorded ABI. Raw
channel/destination pointers and talloc contexts are excluded. Rust allocation
failure follows standard allocator behavior rather than C's NULL return.

Stage 1 covers data/length, headlen/headroom/tailroom, reserve, put/get,
push/pull, trim, reset, invariant, and basic construction/drop equivalents.
Stages 2–3 below add typed/layer helpers and copy/resize. Stage 4 below covers
queues, equality, hex dumps, formatted append, and the context adapter inventory. No msgb endpoint,
GSMTAP-loader integration, or vendored crate snapshot refresh is included.

Regenerate and validate from this repository root:

```bash
./reference/run_msgb_vector.sh > /tmp/msgb_vectors.fresh.json
cmp /tmp/msgb_vectors.fresh.json tests/vectors/msgb/msgb_vectors.json
cmp tests/vectors/msgb/msgb_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/msgb_vectors.json
cmp tests/support/msgb_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/msgb_fixture.rs
./scripts/generate-reference-vectors.sh
cargo fmt --check
cargo test
git diff --check
```

The full generator can reformat legacy packet JSON: compare semantics and
restore unrelated formatting before accepting the diff. Case IDs, input/order,
schema, and count are frozen; changing the contract requires explicit review.

## Message-buffer typed and layer helpers (stage 2)

Stage 2 adds 25 C entry points to `core::msgb`: `msgb_put_u8/u16/u32`,
`msgb_push_u8/u16/u32`, `msgb_get_u8/u16/u32`, `msgb_pull_u8/u16/u32`,
`msgb_l1/l2/l3/l4`, `msgb_sms`, `msgb_l1len/l2len/l3len/l4len`,
`msgb_pull_to_l2`, `msgb_pull_to_l3`, `msgb_l3trim`, and `msgb_push_tl`.
The slash notation expands the related names, e.g. `msgb_put_u16`.
All typed multi-byte operations use big-endian order. Writes return `Result<()>`;
reads return the corresponding unsigned value. Bytes outside written spans
remain unchanged, including bytes removed by typed get/pull operations.

Layer access returns `Option<usize>` backing offsets; SMS aliases L4. Layer
lengths return `Result<u32>`, preserving C's unsigned wrapping subtraction when
a marker is beyond tail, even outside active data. Missing markers return
`MsgbError::MissingLayer` instead of C assertion panics. Pull-to-L2 clears L1;
pull-to-L3 clears L1/L2. Both require a marker in `[data, tail]`, inclusive,
and preserve all state on errors (`MissingLayer` or `LayerOutsideMessage`).

L3 trimming sets total length to `(L3 - data) + l3len`, using a signed
calculation. Negative requested L3 lengths are accepted when the resulting
total is nonnegative. Markers before data or beyond tail may participate in
this defined calculation. Rust rejects missing L3, totals outside signed
32-bit range (`ArithmeticOverflow`), negative totals (`NegativeLength`), and
tails beyond backing storage (`NotEnoughTailroom`), without mutation. C
narrowing outside the signed-int range and NULL pointer subtraction are not
portable fixture cases.

Tag/length prepending adds two bytes: the tag and the prior message length
narrowed to eight bits, including wrap for lengths above 255. It returns the
new data offset. Headroom errors do not write either byte.

A separate 7,572-record C fixture, `msgb_layers_vectors.json`, captures complete
before/after storage and metadata, queries, return values, and panic outcomes.
It covers all 256 u8 values for all four typed operations; u16/u32 boundary
values and high-bit results; exact/insufficient room and message lengths;
layer markers in headroom, active data, and tailroom or unset; wrapping layer
lengths; successful and failed pull-to-layer; signed L3 trim boundaries;
tag and length wrapping; and a 14-step mixed sequence. It shares schema helpers
with stage 1, whose 3,695 records remain byte-for-byte unchanged. Across the two
stages there are 11,267 operation records, excluding setup and snapshot queries.

Successful C u16/u32 pushes use aligned destinations: the C wrappers cast to
typed pointers before byte stores. Rust uses byte slices and supports unaligned
pushes; that extension has separate Rust-only tests. The C fixture excludes
unsafe NULL subtraction, out-of-storage trim, and implementation-defined signed
narrowing. Both projects validate exact schema/provenance/input order, untouched
bytes, typed byte order, query consistency, metadata changes, and corrupt records.
The conversion crate compares Rust results against C and tests safety differences.

The runner's `--layers` mode additionally pins `bits.h`, `utils.h`, the endian
header template, its `Makefile.am` generation rule, and generated `bit16gen.h`
and `bit32gen.h`. The template/rule/bits/utils files were compared byte-for-byte
with upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-08.
The generated headers were reproduced from that template using the verified
`XX` replacement rule and compared byte-for-byte. Their SHA-256 hashes are
pinned in the runner and recorded in each observation. Headers/library still
come exclusively from the required local C checkout.

Stages 1–3 cover storage, typed/layer helpers, and copy/resize.
Stage 4 below completes the adapter inventory with explicit exclusions. GSMTAP consumers, endpoints, and the vendored snapshot are unchanged.

From this repository root, regenerate and verify:

```bash
./reference/run_msgb_vector.sh --layers > /tmp/msgb_layers.fresh.json
cmp /tmp/msgb_layers.fresh.json tests/vectors/msgb/msgb_layers_vectors.json
cmp tests/vectors/msgb/msgb_layers_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/msgb_layers_vectors.json
cmp tests/support/msgb_layers_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/msgb_layers_fixture.rs
./scripts/generate-reference-vectors.sh
cargo fmt --check
cargo test
git diff --check
```

The stage-2 validator in `tests/support/msgb_layers_fixture.rs` uses shared
`msgb_fixture.rs` schema helpers. Both files and both msgb fixtures match the
conversion repository byte-for-byte. `tests/msgb_layers_reference_vectors.rs`
validates structure and corruption rejection; it does not implement Rust APIs
or generate expected outputs. All new fixtures stay outside packet/replay loaders.

## Message-buffer copy and area resize (stage 3)

`core::msgb` adds `msgb_copy(&Msgb) -> Result<Msgb, MsgbError>`,
`msgb_copy_resize(&Msgb, capacity: usize) -> Result<Msgb, MsgbError>`, and
`msgb_resize_area(&mut Msgb, area: usize, old_size: i32, new_size: i32)` returning
`Result<(), MsgbError>`. The two copy functions also represent the buffer
behavior of C's `msgb_copy_c` and `msgb_copy_resize_c`; Rust ownership replaces
context/name arguments rather than reproducing talloc context semantics.

Copies preserve active bytes, data/tail offsets, and optional L1–L4 offsets.
The local C implementation copies only active bytes despite its broader
comment: headroom and unused tailroom bytes in the new allocation are zero.
Control words and raw destination/channel metadata are cleared in C; Rust
clears its control words and has no raw pointer metadata. Copy storage is owned
independently of the source. New capacity includes headroom and must hold the
current tail; Rust also requires preserved layer offsets within that capacity.
Failed copies leave the source unchanged. Rust allocation failure retains the
standard allocator policy rather than C's NULL allocation-failure return.

Area resize accepts an absolute backing offset, requires the old area fully
inside `[data, tail]`, and moves its suffix using memmove semantics. Layer
markers at or after `area + old_size` move by `new_size - old_size`, including
markers in unused storage. Markers inside the removed area remain unchanged,
even if they subsequently lie beyond tail or violate layer ordering. Control
words and C destination/channel metadata are preserved. Zero-size insertion,
whole-message removal, overlap in both directions, and no-op resize are covered.

Growth does not initialize the extension; shrinkage does not erase the unused
suffix. Observations of initialized buffers retain all bytes outside the moved
suffix's destination. This does not promise generally zero-filled extension
contents. Rust checks active storage bounds and every relocated layer before
mutation, rejects negative sizes, and leaves all state unchanged on errors.
A marker that would move beyond backing storage produces `OffsetOutOfRange`;
an old area outside active data produces `AreaOutsideMessage`.

The separate `msgb_resize_vectors.json` holds 8,888 direct C records: 1,131
copy calls across all four wrappers and 7,757 area-resize calls. Successful
copies record both unchanged source state and the complete new buffer, with
pointer identity reduced to a distinct-storage flag. NULL copy results are
explicit (`copy_present=0`), with absent-state sentinels. Resize calls record
before/after bytes, offsets, all layer/control metadata, queries, return codes,
and guarded panics. Inputs/order/schema/provenance and corruption rejection
are validated identically in both projects. All three msgb fixtures total
20,155 operation records; the first two fixtures remain byte-for-byte unchanged.

The C harness excludes pointer arithmetic beyond either allocation, NULL
relational comparisons during mutating resize, and growth that would exploit
C trim's incomplete headroom check. Negative-size panic cases keep C's
pre-check pointers within backing storage. Rust-only tests cover unset layers,
unsafe shrink-copy layer offsets, unsafe growth/relocation, extreme arguments,
copy independence, and preservation of unwritten bytes and removed-area markers.

`reference/run_msgb_vector.sh --resize` pins the existing msgb/panic reference
plus local logging source/header and the assertion header. Logging is initialized
without targets so legitimate copy-failure logging does not cause an unrelated
panic. The logging files were verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-09; the assertion header
was already verified on 2026-10-08. The fixture ABI additionally records 32-bit
signed int. The runner builds, links, and loads only the required local checkout.

Stages 1–3 cover storage, typed/layer helpers, and copy/resize.
Stage 4 below completes the adapter inventory with explicit exclusions. GSMTAP consumers, endpoints,
and the sibling vendored crate snapshot remain unchanged.

From this repository root:

```bash
./reference/run_msgb_vector.sh --resize > /tmp/msgb_resize.fresh.json
cmp /tmp/msgb_resize.fresh.json tests/vectors/msgb/msgb_resize_vectors.json
cmp tests/vectors/msgb/msgb_resize_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/msgb_resize_vectors.json
cmp tests/support/msgb_resize_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/msgb_resize_fixture.rs
./scripts/generate-reference-vectors.sh
cargo fmt --check
cargo test
git diff --check
```

`tests/msgb_resize_reference_vectors.rs` validates the frozen schema/coverage
and corruption rejection through `tests/support/msgb_resize_fixture.rs`, which
matches the conversion crate byte-for-byte. Fixture directories remain separate
from packet/replay loaders. Expected observations come only from direct C calls.
Review full regeneration and restore unrelated formatting-only fixture changes.

## Message-buffer remaining adapters (stage 4)

Stage 4 completes the planned conversion and adapter inventory. Rust queues
own their messages in `MsgbQueue` (`VecDeque<Msgb>`): `msgb_enqueue`,
`msgb_dequeue`, `msgb_queue_free`, `msgb_enqueue_count`, and
`msgb_dequeue_count` preserve FIFO order. Counted operations use a separate
caller-owned `u32` with wrapping arithmetic; an empty dequeue and queue free
leave that counter unchanged. Moving a message transfers ownership, and
queue free drops queued buffers. Rust does not expose intrusive links or allow
the same owned message to be enqueued twice.

`msgb_eq_data` / `msgb_eq_l1_data` through `msgb_eq_l4_data` compare slices;
`msgb_eq` / `msgb_eq_l1` through `msgb_eq_l4` compare messages. They return
`Result<bool, MsgbError>`. `msgb_eq_level` selects levels 0–4, and
`msgb_eq_optional` returns false for absent message/data inputs, matching
C NULL comparisons. Empty slices remain present inputs. Headroom layer bytes
are comparable. Unset or beyond-tail layer comparisons return checked errors;
C's pointer subtraction and potentially enormous wrapped lengths are excluded.

`msgb_hexdump_buf(&mut [u8], &Msgb)` returns a success boolean in place of C's
buffer pointer or `"ERROR"`, preserving defined truncation, NUL writes, and
untouched suffix bytes. `msgb_hexdump` returns an owned `String` with C's
4100-byte output limit. `msgb_hexdump_owned` models `msgb_hexdump_c` using
`length * 3 + 100` bytes without talloc. These retain the underlying
`osmo_hexdump` limit of 1365 input bytes **per segment** and C's layer
annotations, including markers in headroom, behind an earlier marker, or
beyond tail. `msgb_hexdump_l1` through `msgb_hexdump_l4` return `"[]"` for
unset/empty layers and errors for beyond-tail markers. Rust safely checks
short annotation writes where C fails to check `snprintf` truncation; those
unsafe C buffer sizes are excluded from observations. Out-of-allocation layer
pointers cannot be represented by Rust, so C's corresponding annotation has
no callable Rust case.

`msgb_printf(&mut Msgb, format_args!(...))` uses Rust formatting;
`msgb_append_str` appends already formatted UTF-8 bytes. The byte-storage
contract matches C: a spare NUL byte is required, that NUL is outside active
data, and embedded NUL bytes count toward message length. On insufficient
space, it returns `NotEnoughTailroom` (C `-EINVAL`), preserves offsets/layers/
control metadata, and writes truncated output plus NUL into available unused
tail bytes. With no tailroom, no bytes change. This is the documented
exception to the other mutating APIs' unchanged-on-error rule. Rust format
syntax, formatting errors, and allocation failures are not C variadic/locale
formatting compatibility claims.

The explicit allocation and context inventory is:

| C entry points / adapters | Rust equivalent or exclusion |
| --- | --- |
| `msgb_alloc`, `msgb_alloc_c` | `Msgb::new`; zero-initialized owned storage, maximum `u16::MAX`. Names, parent contexts, talloc reporting, and C NULL-on-allocation-failure behavior are excluded; Rust allocation follows its allocator policy. |
| `msgb_alloc_headroom`, `msgb_alloc_headroom_c` | `Msgb::with_headroom`; capacity includes headroom. Excess headroom is an error instead of C assertion/panic. Context/name arguments are excluded. |
| `msgb_free`, `msgb_queue_free` | Rust drop and explicit queue clear/drop. No shared talloc tree lifetime or intrusive linkage. |
| `msgb_copy`, `msgb_copy_c`, `msgb_copy_resize`, `msgb_copy_resize_c` | Stage-3 owned copies preserve the tested buffer semantics. Context/name variants share the Rust owned entry points. |
| `msgb_hexdump_c` | `msgb_hexdump_owned`; owned string replaces talloc output/context. |
| `msgb_set_talloc_ctx`, `msgb_talloc_ctx_init` | Excluded. Rust has no global talloc root or pool; no process-global allocation state is introduced. |
| `_msgb_eq` source-location/print arguments; `msgb_eq_data_print`, `msgb_eq_l1_data_print` through `msgb_eq_l4_data_print` | Comparison behavior is represented by the checked equality helpers. Fatal logging, source locations, and print side effects are excluded. |
| C destination/channel pointers and list links | Excluded from owned `Msgb`; control words remain fixed 64-bit values matching the fixture ABI. |

The separate `msgb_adapters_vectors.json` contains **4,055 direct C records**:
450 slice comparisons (including NULL inputs), 270 message comparisons,
2,942 caller-buffer dumps, 62 static and 62 allocated dumps, 152 layer dumps,
90 formatted appends, and 27 queue operations. Across all four stages there
are **24,210** operation records. Cases cover zero capacity/empty data,
nonzero headroom, full buffers, exact dump size thresholds, reordered/unset
layers, 1364/1365/1366-byte dump limits, 3000-byte segmented output, FIFO
sequences, and unsigned counter underflow/overflow.

The shared `msgb_adapters_fixture.rs` validates strict schema, frozen input
coverage, ABI/source provenance, complete backing and metadata snapshots,
queue state/counters, equality outcomes, append writes, and dump destination
integrity. A frozen FNV-1a fingerprint of the direct C file additionally
rejects all changes to observed output bytes; it is an integrity check, not a
cryptographic authenticity claim. Corruption tests cover missing/duplicate/
unknown fields, provenance, ABI, outcomes, backing bytes, dump output,
counters, and missing records. Rust conformance checks every record; separate
safety tests exercise unsafe C annotation sizes and invalid layer spans.
Ordinary Cargo tests require no C installation.

`reference/run_msgb_vector.sh --adapters` uses only the required local C
checkout. It additionally pins `src/core/utils.c`, `utils.h`, and `linuxlist.h`;
new source/list dependencies were verified byte-for-byte against revision
`950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-09. The queue harness
uses GNU C11 because the upstream list macros use `typeof`. Its libtalloc
link supplies the existing C library's allocation dependency. Fixtures and
validators are shared byte-for-byte between repositories.

All four planned stages are complete with the explicit exclusions above;
this is not a claim of talloc, logging, raw-pointer, or C printf ABI parity.
GSMTAP consumers, endpoints, and the vendored crate snapshot are unchanged.
No snapshot refresh is required for these independent conformance fixtures.

Regenerate stage 4 from the testing repository:

```bash
./reference/run_msgb_vector.sh --adapters > /tmp/msgb_adapters.fresh.json
cmp /tmp/msgb_adapters.fresh.json tests/vectors/msgb/msgb_adapters_vectors.json
cmp tests/vectors/msgb/msgb_adapters_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/msgb_adapters_vectors.json
cmp tests/support/msgb_adapters_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/msgb_adapters_fixture.rs
```

`./scripts/generate-reference-vectors.sh` includes all four msgb runners.
When intentionally revising C cases, review every changed observation and
update the frozen input manifest/count/fingerprint from direct C output.

## Primitive observations

`prim_vectors.json` contains **6,865 direct C records**: 2,112 initialization
and reinitialization records, 4,752 event-map records, and one callback record.
The initialization matrix combines six SAPs (including all three namespace
bases), eight primitive IDs, eleven operation values, both message attachment
states, and two initialization phases. Phase two complements every numeric
field and toggles attachment. This covers zero/max values, packed-ID overflow,
low-byte operation collisions, all four known names, and unknown-name fallback.
Event cases cover empty sentinel maps, exact and single-field mismatches,
duplicate first matches, sentinel-before-match, zero events, and `UINT32_MAX-1`.
Full raw operations must match even when packed low bytes coincide.

Every init/event record snapshots all 16 backing bytes, data/tail/length,
all four layer offsets, and all five control words. Attachment is checked by
identity. The callback record observes header/payload/context mutation and a
negative return through the C callback typedef. Unsafe pointers, unterminated C
maps, and raw embedded-header layout are not C fixture inputs; Rust separately
tests bounded maps and stable unknown-name text.

The shared validator checks exact schema, numeric canonicalization/ranges,
ordered input coverage, map entries, ABI, source hashes, and a frozen FNV-1a
fingerprint of the complete C output. This fingerprint protects fixture integrity,
not cryptographic authenticity. Corruption tests reject missing/duplicate/unknown
fields, altered provenance/ABI/inputs/maps/output/association/names, missing or
extra records, and malformed framing. Ordinary Cargo tests require no C build.

`reference/run_prim_vector.sh` verifies SHA-256 hashes for `prim.c`, `prim.h`,
`utils.c`, `utils.h`, `msgb.c`, and `msgb.h`, verified byte-for-byte against
revision `950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-09. It builds,
links, and selects runtime libraries only from the required local C checkout.
It requires the observed ABI: 32-bit unsigned int/operation enum and 64-bit
unsigned long/pointers. It fails on source or ABI mismatch.

Regenerate from the testing repository:

```bash
./reference/run_prim_vector.sh > /tmp/prim.fresh.json
cmp /tmp/prim.fresh.json tests/vectors/prim/prim_vectors.json
cmp tests/vectors/prim/prim_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/prim_vectors.json
cmp tests/support/prim_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/prim_fixture.rs
```

`./scripts/generate-reference-vectors.sh` includes the prim runner. Fixtures and
validators are shared byte-for-byte. Review all regeneration diffs; update the
frozen coverage/fingerprint only from reviewed direct C observations. These
independent fixtures require no vendored crate snapshot refresh.

## Core CRC observations

The CRC conversion covers byte-oriented CRC-16/CCITT and generic parameterized
8/16/32/64-bit CRCs. GSM parity/channel definitions and convolutional coding
are outside its scope.

`crc_byte_vectors.json` contains **3,472 records**: all 512 table entries,
2,560 single-byte calls covering every byte and five initial states, and 400
buffer scenarios. Each buffer scenario directly calls whole-buffer, first-chunk,
and continuation APIs, records all 260 input bytes before/after, and includes
an untouched suffix. The zero-length buffer calls use a null C input pointer.

`crc_generic_vectors.json` contains **22,848 records**. The frozen matrix covers
every configured width in each storage type, four neutral descriptors, four
input patterns (including nonbinary bytes), lengths `INT_MIN`, -1, 0, 1, 7,
and 32, and exact/oversized CRC destinations. Each record directly observes
compute, set, matching check, a single-bit mismatch at every position, and a
nonbinary CRC byte at every position. Complete input and destination bytes are
recorded before/after, with canaries preserving destination suffixes.

The local C template's positive-input full-width 32/64-bit mask invokes undefined
shifts. Those calls are excluded from golden fixtures. Their zero/negative-length
calls are defined and included. Full-width CRC-8/16 is defined on the pinned ABI
through integer promotion and is included for all lengths. The `--audit` mode
compiles the exact local CRC sources with UBSan, verifies the whole golden domain
is clean, and requires diagnostics for full-width 32/64 positive-input shifts,
zero width, and `INT_MIN` width subtraction overflow. Diagnostics are not golden
CRC results and the C source is never patched.

Rust accepts widths 1..=storage width, defines full-width 32/64 masks safely,
and rejects all other widths before arithmetic/access. Nonpositive input lengths
return init XOR remainder; check/set still need the configured CRC slice. Input
bytes use only their low bit; supplied CRC bytes are compared exactly against
0/1. High descriptor field bits and high final-XOR bits are preserved. Bounds
errors leave destinations unchanged. Full-width positive-input Rust behavior
is tested with hand-traced single-bit cases and independent u128 polynomial
division, outside C fixtures.

The strict shared validator checks exact unsigned `u64` values (including
`u64::MAX`) and signed `i32` lengths, schema/canonical numbers, provenance,
ordered coverage, unchanged inputs, full output state/suffixes, and a frozen
FNV-1a fingerprint of all direct C output. The fingerprint checks integrity,
not cryptographic authenticity. Corruption tests cover fields, framing,
revision/hashes/ABI, numeric overflow, coverage, observed results, mismatch
records and output bytes; separate parser tests exercise validation independently
of the fingerprint. Rust conformance compares every observation.

`reference/run_crc_vector.sh` pins 16 local source/header/template/build-rule
and generated-file hashes. Nongenerated files were verified byte-for-byte at
revision `950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-09. All eight
generated CRC files match the verified templates' `XX` substitutions. Build,
link and runtime selection use only the required local C checkout; mismatches
fail. The observed ABI requires 32-bit `int` and 8-bit bytes.

Regenerate and audit from the testing repository:

```bash
./reference/run_crc_vector.sh byte > /tmp/crc-byte.fresh.json
./reference/run_crc_vector.sh generic > /tmp/crc-generic.fresh.json
./reference/run_crc_vector.sh --audit
cmp /tmp/crc-byte.fresh.json tests/vectors/crc/crc_byte_vectors.json
cmp /tmp/crc-generic.fresh.json tests/vectors/crc/crc_generic_vectors.json
cmp tests/vectors/crc/crc_byte_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/crc_byte_vectors.json
cmp tests/vectors/crc/crc_generic_vectors.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/crc_generic_vectors.json
cmp tests/support/crc_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/crc_fixture.rs
```

The full generator includes both CRC stages. Fixtures and validators are shared
byte-for-byte; ordinary Cargo tests require no C build. Review every generation
diff, including existing fixtures. Update fingerprints only from reviewed direct
C evidence. No vendored crate refresh is needed for independent fixture support.

## Convolutional evidence and backend gate

`conv_vector.c` calls all 17 selected core convolutional exports directly.
`run_conv_vector.sh all` emits 36,256 flat NDJSON records: four metadata,
1,066 descriptor and 35,186 operation records across 1,707 case identifiers.
The linked public dispatcher stream and forced isolated scalar/SSE/AVX streams
are labeled separately. Snapshots contain semantic state and valid history,
not C memory layout, addresses or uninitialized allocation bytes. Full inputs
and initialized/final destinations retain suffixes. Expected bits, metrics and
returns come from C only; the harness does not implement a decoder.

Unlike the older runners above, this runner never rebuilds the source tree.
It uses the existing pinned local library for public dispatch and temporary
exact-source builds for isolated backends. The linked dispatcher observations
are pinned-local-binary evidence, not proof that the existing binary was built
from the pinned sources. Exact-source scalar/SSE/AVX observations remain
separately identified. The required untracked local
snapshot has no `.git`; upstream Git revision is `unknown`. Embedded version
`1.14.1.1-95043` is descriptive, not proof of a revision.
`conv_provenance.json` pins 33 participating source/header/template/build/version
files plus the existing library. `conv_generator_provenance.json` pins the
harness, runner, reference manifest and schema/coverage. Missing or mismatched
files, unpinned transitive project includes and external library aliases fail
closed. No system library or other checkout is substituted.

The matrix covers small exhaustive input, larger seeded/structured patterns,
rates `1,2,3,4,5,8,9`, constraints `2,3,5,7,9,10`, termination `0,1,2,42`,
recursion, puncturing, soft erasures/noise/ties/`-128`, normalization boundaries,
state reuse/reset/rewind and two generated length-97 synthetic replacement cases.
State loading packs and narrows bits instead of walking transitions. The length
helper subtracts all puncture entries, even beyond the stream, so it can differ
from actual write counts and can return defined negative lengths. The approved
Rust compatibility helper preserves these in a checked signed
`Result<i32, ConvError>`. Safe actual output capacity is a separate checked
`usize` result derived from the operation, current stream position and puncture
cursor, never from casting that signed helper. Generic metrics are squared/scaled
errors; correlation
success is status zero. Never combine those semantic expectations.

Available scalar/SSE/AVX decoded outputs and status returns agree for the frozen
comparable cases. The linked library's actual runtime pointer identifies AVX
through pinned ELF symbols. `all` withholds combined output on material backend
divergence; individual modes retain separate observations. NEON's strict tie
comparison differs in inspected source and was not executed, so NEON is outside
the claimed backend-parity boundary. Review has approved the observed
scalar/SSE/AVX tie behavior (scalar `>=`, including the equality choice) as the
portable Rust correlation contract, independent of CPU. No universal backend
equivalence is claimed. The conversion repository now implements checked
descriptors, signed compatibility lengths, independent operation capacities and
reusable/one-shot encoding, with conformance in `tests/conv/conv_encoder.rs`,
plus the generic stateful squared-error decoder, portable scalar correlation
and public one-shot dispatch. `osmo_conv_decode_acc` and `osmo_conv_decode`
take `(&ConvCode, &[i8], &mut [u8])` and return `Result<i32, ConvError>`:
correlation returns `Ok(0)`, generic dispatch returns accumulated error bounded
by `MAX_AE`. `K=5/7 && N<=4` routes to correlation, including `N=1` rejected
there without fallback. Direct correlation requires `N=2..=4`, `K=5/7` and
positive lengths. Rust implements the approved scalar tie choice without SIMD
selection or NEON parity. The existing
frozen matrix covers this stage without C harness, evidence or shared-reader
changes. Additional Rust-only safety tests never generate expected C values.

`ConvDecoder::new(&code, len: i32, start_state: i32)` returns
`Result<ConvDecoder<'_>, ConvError>`. Positive lengths override the descriptor;
nonpositive lengths use it. Stateful methods are `reset(start_state)`, `rewind()`,
`scan(input: &[i8], symbols: usize)`, `flush(input: &[i8])`,
`best_end_state() -> Result<usize, ConvError>` and
`get_output(output: &mut [u8], has_flush: bool, end_state: i32) -> Result<u32, ConvError>`.
Reset/rewind return checked unit results; scan/flush return consumed soft-byte
counts. `has_flush` omits the last `K-1` trellis symbols during traceback.
The seven C-named wrappers are `osmo_conv_decode_init`, `osmo_conv_decode_reset`,
`osmo_conv_decode_rewind`, `osmo_conv_decode_scan`, `osmo_conv_decode_flush`,
`osmo_conv_decode_get_best_end_state` and `osmo_conv_decode_get_output`;
ownership and `Drop` replace `osmo_conv_decode_deinit`.

Init/reset accepts negative starts to enable all states, `0..state_count` to
enable one state, and equality to disable every survivor with `MAX_AE`. C reset
uses an equality loop, so `start_state == n_states` is safe frozen no-survivor
evidence, not an out-of-bounds access. Rust rejects larger nonnegative starts
with `InvalidState`. Negative ends request automatic selection; nonnegative
ends must be strictly below `state_count`, so equality is `InvalidState`.
Generic decoding accepts `N=1..=31`, preserving zero byte masks above eight.
Descriptor state counts use `K=1..=31` with byte predecessors, not wide-state
traceback. `u32` metrics retain signed C `INT_MAX` arithmetic bounds and the
strict `MAX_AE=0x00ffffff` survivor cutoff. Resolved `len + K - 1` and total
history cell count must fit `INT_MAX`, in addition to host-size checks.
`ConvError` variants `InvalidState`, `InvalidDecoderState`, `InvalidOperation`,
`MissingHistory`, `UnreachableTraceback`, `NoSurvivor`, `ArithmeticOverflow`
and short-slice/descriptor
errors preserve reusable state and output; `AllocationFailed` reports fallible
`try_reserve_exact` failures. Successful writes preserve destination suffixes.

Read-only accessors are `frame_length`, `state_count`, `output_index`,
`puncture_index`, `accumulated_errors` (`&[u32]`), `next_accumulated_errors`
(`Option<&[u32]>`, initialized diagnostic scratch only) and `predecessor_history`
(`&[Option<u8>]`, active rows with missing predecessors marked `None`).
Reset/rewind invalidate active history; retained scratch is not another set of
active paths. `scan_input_capacity(symbols)` and `flush_input_capacity()` give
checked soft-byte requirements at the current stream/puncture position.
The frozen stateful subset contains 332 cases and 7,257 lifecycle observations:
6,593 callable API observations plus 664 C deinitializations modeled as drops.

`ConvCode::decode_input_capacity() -> Result<usize, ConvError>` counts actual
encountered punctures and does not impose the encoder's short tail-biting loading
requirement. Nonsystematic recursion maps to `UnsupportedRecursiveCode` (C
`-EPROTO`); negative traceback statuses map to `NoSurvivor` (`-EPROTO`
correlation, `-1` generic). All `i8` values, including `-128`, are accepted;
selected metric narrowing or normalization differences outside `i16` produce
`ArithmeticOverflow`, not implementation-defined C narrowing. Signed C/host
sizing and history arithmetic are checked with fallible allocation. All
one-shot errors preserve output; success preserves unused suffixes.

Independent final-stage replays in the conversion repository's
`tests/conv/conv_correlation.rs` passed 6,796 direct correlation observations
(1,699 each linked/scalar/SSE/AVX) and 6,608 public dispatch observations
(1,652 each), totaling 13,404 full-buffer, status/metric and unchanged-input
comparisons. These are separate from this utility's integrity checks below.
Frozen fixtures, shared readers, manifests and harnesses remain unchanged.

Final full suites passed: conversion repository 130 tests (64 unit, 65 core
integration, one GSMTAP integration) and this utility 85. The final stage adds
13 private unit safety/semantics tests and two independent frozen replays.
Actual final-stage versions were `rustc 1.97.1`, `cargo 1.97.1` and
`clippy 0.1.97`, not the misleading `1.85.0` directory labels; CI Rust 1.70
was not installed or tested locally. IDE build, format and diff-whitespace
checks passed; all-target Clippy had no new correlation diagnostics, while
known global warnings remain unchanged. Shared readers and fixtures compared
byte-for-byte, with no Git diff to frozen fixtures.

Final-stage external validation passed `--verify` with digest
`4dd99c1d7e2295064dd76e5e2f0666ac967ba857c804ce824fff97bd624fe40f`,
isolated scalar/SSE/AVX `--audit`, and two full 36,254-record regenerations
matching both committed fixtures and each other. Reader/manifest comparisons
also passed. Artifacts reside in ignored `target/conv-final-doc-validation/`;
no linked-library instrumentation, NEON execution or arbitrary-backend parity
is claimed. Allocation failure and out-of-domain arithmetic remain Rust-only
safety coverage, not golden C observations.

`tests/support/conv_fixture.rs` is shared byte-for-byte with the conversion
repository; `tests/conv_reference_vectors.rs` checks schema/provenance,
operations/state/buffers, frozen coverage/digest, corruptions and available
backend comparisons. It does not implement or validate Rust convolutional
engines. Rust-versus-C encoder/decoder comparisons remain in the conversion
repository, not this utility's packet/replay loaders. Ordinary Cargo tests require no C
installation. No unreviewed vendored Rust snapshot refresh is included.

From this repository:

```bash
./reference/run_conv_vector.sh --verify
mkdir -p target/conv-stateful-validation
TMPDIR="$PWD/target/conv-stateful-validation" ./reference/run_conv_vector.sh --audit
for pass in 1 2; do
  TMPDIR="$PWD/target/conv-stateful-validation" ./reference/run_conv_vector.sh all > "target/conv-stateful-validation/conv.$pass.ndjson" || exit
  cmp "target/conv-stateful-validation/conv.$pass.ndjson" tests/vectors/conv/conv_vectors.ndjson || exit
  cmp "target/conv-stateful-validation/conv.$pass.ndjson" ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/conv/conv_vectors.ndjson || exit
done
cmp target/conv-stateful-validation/conv.1.ndjson target/conv-stateful-validation/conv.2.ndjson
# Readers have the same integrity checks, but rustfmt follows each crate's edition.
# Evidence and manifests below must match byte-for-byte.
cmp reference/conv_provenance.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/conv/conv_provenance.json
cmp reference/conv_generator_provenance.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/conv/conv_generator_provenance.json
```

Individual evidence modes are `linked-dispatch`, `scalar`, `sse` and `avx`.
The `--audit` mode instruments isolated scalar/SSE/AVX golden paths with
ASan/UBSan and requires separate invalid-shift/signed-sizing-overflow diagnostics
without golden output. The linked library is not sanitizer-instrumented.
Unsafe short tail-biting/empty non-flush traceback and unreachable-history
cases are excluded, not accepted as golden behavior. See the sibling
[`osmocore-rs/src/conv/README.md`](../../libosmocore-rs/osmocore-rs/src/conv/README.md)
for detailed findings and safety boundaries.

The full generator includes `all`, atomically replacing the convolutional
fixture only after success, but overwrites fixtures and other runners rebuild
the C tree. Do not use it for this read-only evidence verification. The commands
above regenerate the full convolutional matrix twice without overwriting fixtures
and keep temporary exact-source builds inside this repository via `TMPDIR`.
Review intentional evidence changes before updating shared copies, manifests
or frozen reader fingerprints. No vendored Rust snapshot refresh,
workbench endpoint, packet/replay integration or commit is part of this stage.

### Independent synthetic replacement and lineage (2026-10-13)

The replacement cases are `synthetic-rate2-k7-l97-unpunctured` and
`synthetic-rate2-k7-l97-punctured`, with six soft modes each. Their parameters
were independently selected for this test using the existing local synthetic
rule, not copied from another upstream or standard vector and not chosen to
recreate the removed protocol tables. For N=2, K=7 the local rule
`(1 << (K-1)) | 1 | (j % 2 ? 2 : 4)` gives masks `0x45, 0x43`.

For state `s=0..63` and input `b=0..1`, `register=(s<<1)|b`,
`next_state=register&63`, and
`output=(parity(register&0x45)<<1)|parity(register&0x43)`. There are no explicit
recursive termination tables; flush uses six zero-input steps. The unpunctured
case has a null puncture list. The punctured case omits raw positions
`0,3,...,204` in the 206-symbol stream, ending its C list with `-1`.

Both paired cases start a uint32 LCG at the new seed `0x8f32d6a1`; before each
bit, `seed=(1664525*seed+1013904223) mod 2^32`, then `input=seed>>31`, for 97 bits.
Their shared hard input SHA-256 is
`65c593a6bed7222d2c04961409d7953ab0cef862c4df92eb4bebbd9941f878ef`. The new input
and output tables were checked against the removed cases; neither input matches
an old vector or its equal-length prefix, and the output tables differ.

Producers in this repository are `reference/conv_vector.c`, `reference/run_conv_vector.sh`,
`reference/conv_provenance.json`, and `reference/conv_generator_provenance.json`.
The existing mathematical helper generates descriptors and inputs only. Direct
local C calls produce encoding/decoding results, metrics, history, returns and
buffer observations. Rust never supplies expected encode/decode outputs.
Conversion-repository consumers are the three `tests/conv/conv_{encoder,decoder,correlation}.rs` replays,
`tests/conv/conv_reference_vectors.rs` and `tests/support/conv_fixture.rs`, plus
this repository's `tests/conv_reference_vectors.rs` and `tests/support/conv_fixture.rs`.
Both repositories retain identical NDJSON and provenance-manifest bytes.

Two full focused generations matched byte-for-byte across linked dispatch and
isolated scalar/SSE/AVX. All unrelated descriptor/operation records are unchanged;
only the two replacement families and producer metadata changed. The existing
recursive/systematic and tail-biting cases remain. Isolated sanitizer audits
passed, including the separate excluded UB diagnostics; linked-library
instrumentation and NEON remain outside the claim.

Historically `upstream-gmr` and `upstream-wimax` used GMR-1/WiMAX fragments
extracted from `libosmocore/tests/conv/conv_test.c`, including packed input and
expected output arrays. Those active records, assertions, source extraction and
temporary `conv_upstream.h` were removed. No upstream vector source is read by
the current runner. The old provenance remains documented without retaining
copied fixture content. The two protocol cases have been replaced, rather than
retaining protocol-specific coverage claims.

Use the focused commands above for this replacement. Never run the broad
reference generator to regenerate unrelated fixtures or rebuild the C checkout.

## Time and timer evidence (Stages 1–2)

`run_time_vector.sh` builds the unmodified required local clock/timer/rbtree
sources into temporary executables. It never rebuilds the source tree or links
an installed libosmocore. `time_provenance.json` pins 13 local participating
source/header/configuration/build/test files and 94 platform headers; the runner
also checks transitive includes. `time_generator_provenance.json` pins the harness
and runner. Upstream Git revision is unknown; observations are exact-source
evidence on the selected 64-bit Linux, nonembedded ABI.

`tests/vectors/time/time_vectors.ndjson` has 20,933 deterministic records:
20,736 raw timeval/timespec operations, 44 clock records, 48 seconds helpers,
72 timer trace records, 32 timer API records and one metadata record. Fractional fields span and exceed
one/two-second boundaries in both directions. Operations include in-place aliases
and direct override-add. No sleeps or expected results computed in Rust are used.
The shared validator freezes schema, ordered coverage, provenance and full
observations, and rejects corrupt fixtures. It contains no timing implementation.

The timer cases include self/peer mutation, ordinary deferred immediate timers,
a callback calling a nested update, a callback adding an immediate timer then
nesting an update, and prepare/nearest during dispatch. A separate bounded
nested-peer probe demonstrates repeated outer callbacks and exits 77 after
12 callbacks; this pathological probe is never golden fixture data. The future
Rust scheduler will intentionally reject nested updates while supporting
prepare/nearest during callbacks. Stage 2 also records self/peer callback
replacement, remaining equality and raw boundaries, relative scheduling,
nearest rounding through INT_MAX, absolute lifecycle/cache behavior, missing
callbacks, and LONG_MAX dispatch. LONG_MAX suppresses only the nearest cache,
not activation or dispatch. Stage 1 converts clock/time primitives; Stage 2
implements the owned scheduler in the sibling conversion crate.
The planned TDef claim is core TDef parity excluding FSM integration.

Verify/regenerate without overwriting committed evidence:

```bash
./reference/run_time_vector.sh --verify
./reference/run_time_vector.sh --audit
./reference/run_time_vector.sh all > /tmp/time.1.ndjson
./reference/run_time_vector.sh all > /tmp/time.2.ndjson
cmp /tmp/time.1.ndjson /tmp/time.2.ndjson
cmp /tmp/time.1.ndjson tests/vectors/time/time_vectors.ndjson
cmp tests/vectors/time/time_vectors.ndjson ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/time/time_vectors.ndjson
cmp tests/support/time_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/time_fixture.rs
cmp tests/vectors/time/time_provenance.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/time/time_provenance.json
cmp tests/vectors/time/time_generator_provenance.json ../libosmocore-rs/osmocore-rs/tests/compatibility/vectors/time/time_generator_provenance.json
cargo fmt --check
cargo test
```

`--audit` runs golden calls and the bounded nested-peer probe with ASan+UBSan,
then requires signed-overflow diagnostics for seconds add/sub, carry, borrow and
fraction addition. These diagnostics never become expected outputs. LSan is
disabled because the tracing environment prevents it. The runner fails closed
on source/header/generator mismatch and withholds stdout until all probes pass.
The full generator includes the time runner and publishes its output atomically;
use the targeted commands above for this block to preserve unrelated fixtures.
Review any changed C records before updating fingerprints and shared copies.
No vendor snapshot update or packet/replay integration is included.

The conversion project's Stage 1 replay compares 20,828 C observations; this
utility validates evidence only. Stage 2 replays 57 nonrecursive timer trace
records and 32 timer API observations in the conversion project; 15 recursive
C records remain policy evidence, with separate Rust rejection tests.
See [time compatibility](../../libosmocore-rs/osmocore-rs/src/time/README.md) for
Rust API shapes, safety boundaries and reentrant-dispatch findings.

## TDef core evidence (Stage 3)

`run_tdef_vector.sh` builds unmodified `tdef.c`, `utils.c`, `panic.c`, and
`backtrace.c` from the required local checkout. LTO and section garbage
collection discard unused FSM/logging paths; no FSM stubs, installed
libosmocore library, or source-tree build is used. The closed SHA-256 manifest
pins 24 local source/header/configuration/test files and 106 platform headers,
checks transitive includes, and separately pins harness and runner sources.
There is no upstream Git metadata: evidence is exact-source, revision unknown.
The selected ABI has 64-bit unsigned long/long and 32-bit int. Stage 3 parity
uses Rust u64 against this recorded environment; 32-bit ULONG_MAX behavior is
outside the current claim.

The golden NDJSON has 2,175 records: one metadata record; 900 supported unit
conversion get/set pairs; 30 safe invalid-unit pairs; 81 zero-valued unit pairs;
seven lookup/mutation traces; one reset trace; 14 missing-table cases; four
sentinel/null traversal cases; 360 range/set cases; 34 valid reset boundary
cases; 408 complete format buffer observations; nine unit names; six unit-name
table entries including its terminator; and 320 state-timeout observations.
All expected outputs come from direct C calls. `tables()` directly calls
`osmo_tdef_get_entry()` and records its returned index for signed/zero IDs,
duplicate first matches, missing IDs and hidden entries after the sentinel.
Null and hidden-traversal cases also call it directly. This testing utility
continues to validate integrity/coverage only; the sibling Rust conversion
crate now implements core TDef and replays all records against the frozen C
observations, with explicit safety exceptions for assertion/UB cases.

`tdef_diagnostics.json` is separate diagnostic evidence, never a golden numeric
conversion result. It records 95 isolated probes: three invalid-default panic
snapshots, three real assertion aborts, 88 sanitizer-diagnosed division-by-zero
calls (44 invalid pairs through get and set), and one null-buffer write. Reset
snapshots use C's supported panic handler and exit 77 without returning from the
handler. Ordinary negative-fallback and invalid-state probes use the default
panic path and require SIGABRT. The runner disables core dumps and bounds each
probe to five seconds. ASan+UBSan must find no errors in the golden domain;
LSan is disabled under the tracing environment. Unstable addresses, backtrace
text, and executable paths are not published as diagnostic fixture data.

Formatting with declared capacity zero uses a 64-byte initialized backing
array, so the C buf[0] write is valid and observed. The separate null-pointer
probe is unsafe and sanitizer-diagnosed. An empty Rust slice returns
the required length without any write. The sibling replay checks this exception
and compares positive capacities byte-for-byte, including the untouched suffix.
Unknown same-unit/custom conversions, all zero conversions, and missing lookup
fallbacks with unknown units are defined. Positive conversions whose two
factors are both zero return typed Rust errors. Core Rust set uses typed results;
its compatibility wrapper maps missing definitions to recorded C -EEXIST (-17)
and rejected values to -ERANGE (-34). Assertion/UB equivalents keep typed errors,
without invented C return codes. Reset failure preserves the reset prefix. FSM
execution, helper/macros, VTY, and `_osmo_tdef_fsm_inst_state_chg()` remain deferred.

Non-destructive validation/regeneration from this repository:

```bash
./reference/run_tdef_vector.sh --verify
./reference/run_tdef_vector.sh --audit
./reference/run_tdef_vector.sh all > /tmp/tdef.1.ndjson
./reference/run_tdef_vector.sh all > /tmp/tdef.2.ndjson
./reference/run_tdef_vector.sh --diagnostics > /tmp/tdef.diagnostics.1.json
./reference/run_tdef_vector.sh --diagnostics > /tmp/tdef.diagnostics.2.json
cmp /tmp/tdef.1.ndjson /tmp/tdef.2.ndjson
cmp /tmp/tdef.1.ndjson tests/vectors/tdef/tdef_vectors.ndjson
cmp /tmp/tdef.diagnostics.1.json /tmp/tdef.diagnostics.2.json
cmp /tmp/tdef.diagnostics.1.json tests/vectors/tdef/tdef_diagnostics.json
cmp tests/support/tdef_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/tdef_fixture.rs
cargo fmt --check
cargo test
```

Also compare all four files in `tests/vectors/tdef/` byte-for-byte with the
conversion project's `tests/compatibility/vectors/tdef/`, and both provenance
manifests with the copies in `reference/`. Review observations before changing
reader fingerprints. The full generator includes TDef and stages both outputs
before publishing; use targeted regeneration for a TDef-only update to preserve
unrelated fixtures. No vendor snapshot or packet/replay loader changes belong
in this stage. See the conversion repository's `osmocore-rs/src/time/README.md` for
implemented scope, replay and safety policies. No vendor snapshot was refreshed.

## time_cc evidence (Stage 4)

`run_time_cc_vector.sh` compiles the required unmodified local `time_cc.c`,
`timer.c`, both clock shims, rbtree, TDef, rate counter, utils, panic and backtrace
sources with GNU C11, O2, LTO and section garbage collection. Discarded paths
need no FSM, logging, select, rate-group or talloc replacement stubs/libraries.
The real `rate_ctr_add()` is used against initialized local `struct rate_ctr`
storage; previous/interval fields are checked untouched. Existing upstream
unused-parameter/sign-comparison warnings are suppressed without editing sources.

The exact-source manifest pins 30 local files and 128 platform headers, verifies
transitive includes, and records unknown revision (the snapshot has no Git
metadata). Generator provenance pins the harness/runner hashes and the manifest.
Includes and links never use an installed libosmocore. The only fault injection
is linker wrapping **libc** `clock_gettime()` to return EIO with public monotonic
overrides disabled; no libosmocore API is substituted. Unexpected unoverridden
native reads abort the harness. Ordinary reads use public clock overrides.

The ABI is 64-bit unsigned long/long/time fields, 32-bit int, eight-bit bytes.
Unsigned time and counter arithmetic wraps modulo 2^64. Timer-relative seconds
narrow via GCC's low 32 bits to signed int on this recorded target; this is
implementation-defined C behavior, not a portable C or 32-bit ABI claim.

Golden NDJSON contains one metadata record and 5,947 trace records. A 448-case
matrix combines eight granularity values, seven thresholds, four forgetting
values and presence/absence of a sink. The remaining 21 traces isolate clock
skew/delay, cleanup/reuse, zero start, EIO fallback, configuration, TDef changes,
missing sink, wrapping, report caps and UINT64_MAX next-event branches. `seed`
operations initialize public accounting fields to representable boundary states;
they are edge probes, not normal-lifetime histories. Outputs are only direct C
observations: internal sums/configuration/counter, active status, full deadline,
registered timer count and dispatch work. The shared reader freezes schema,
ordered inputs and all outputs separately from any Rust model.

Separate diagnostics contain null init/set/cleanup, invalid TDef unit division
by zero and missing-TDef assertion. Each child is bounded to five seconds;
assertions require SIGABRT and UB cases require actual sanitizer diagnostics.
Core dumps and LSan under ptrace are disabled. Successful traces must be
ASan+UBSan clean, including the intentional unsigned wrapping/narrowing edges.
Monotonic EIO is a defined zero-timestamp outcome and belongs in successful
traces, not an invented C failure status. Rust converts TDef failures to typed
errors and uses references to exclude null-object calls. Wall-clock failures
in timer scheduling have no safe C numeric oracle (the return is ignored);
Rust-only tests exercise typed errors there.

Non-destructive validation from this repository:

```bash
./reference/run_time_cc_vector.sh --verify
./reference/run_time_cc_vector.sh --audit
./reference/run_time_cc_vector.sh all > /tmp/time_cc.1.ndjson
./reference/run_time_cc_vector.sh all > /tmp/time_cc.2.ndjson
./reference/run_time_cc_vector.sh --diagnostics > /tmp/time_cc.diagnostics.1.json
./reference/run_time_cc_vector.sh --diagnostics > /tmp/time_cc.diagnostics.2.json
cmp /tmp/time_cc.1.ndjson /tmp/time_cc.2.ndjson
cmp /tmp/time_cc.1.ndjson tests/vectors/time_cc/time_cc_vectors.ndjson
cmp /tmp/time_cc.diagnostics.1.json /tmp/time_cc.diagnostics.2.json
cmp /tmp/time_cc.diagnostics.1.json tests/vectors/time_cc/time_cc_diagnostics.json
cmp tests/support/time_cc_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/time_cc_fixture.rs
cargo fmt --check
cargo test --target-dir /tmp/stage4-gsmtap-target
```

Compare all four `tests/vectors/time_cc` files with the Rust crate's dedicated
fixture directory and both manifests with `reference/`. Run the time and TDef
reference verification/audits/regenerations as part of combined Stage 1–4
validation. The full generator now stages time_cc's two outputs before
publishing either. Use the targeted commands above for a time_cc-only refresh,
preserving unrelated fixtures; never update frozen fingerprints just to pass.
The sibling behavior replay includes all four stages and an additional shared
scheduler/application-timer integration check. FSM/TDef integration and VTY
remain deferred. Vendor snapshots and packet/replay loaders are unchanged.

## Standalone rate-counter evidence (Stage 5a)

`run_rate_ctr_vector.sh` compiles only the unmodified required local
`src/core/rate_ctr.c` with the direct-call `rate_ctr_vector.c` harness. LTO,
function/data sections and linker garbage collection discard unused registry,
logging and timerfd/select dependencies; observed APIs are never stubbed.
The closed runner verifies source/platform hashes, all transitive includes,
compiler version/target, and the generator manifest before compilation. It
clears compiler/library search overrides. Revision is `unknown` for the required
snapshot without Git metadata, never the parent repository revision.

The successful TSV domain has one metadata line and 1,700 calls in 100
scenarios. Columns are scenario, step, operation, signed increment, signed
return (zero placeholder for void calls), then ten initialized and ten final
state words. State order is current, previous, and last/rate pairs for second,
minute, hour and day. Initial current/previous form a 10-by-10 boundary matrix;
sequences directly call difference twice, seven signed additions, inline inc,
difference twice, reset, difference, add(-1), difference and reset. Zero,
positive/negative values, INT_MIN/INT_MAX, INT64_MAX/high-bit boundaries and
UINT64_MAX cover wrapping, signed conversion, repeated reads, and every
preserved interval field. Unsigned-to-int64 conversion is implementation-defined
in C and frozen for the recorded GCC/64-bit Linux ABI, not claimed universally.
Four bounded isolated null add/inc/difference/reset probes are separate
sanitizer diagnostics; no unsafe probe contributes golden output.

Dedicated files in `tests/vectors/rate_ctr/` and the shared independent reader
`tests/support/rate_ctr_fixture.rs` must match the sibling conversion crate.
This repository validates integrity, provenance and corruptions; Rust API and
complete state replay belongs in `osmocore-rs`. The conversion also replays
all frozen time_cc scenarios with `TimeCc<RateCtr>` and checks that previous and
every interval field survive reporting. Full registry, interval scheduling,
initialization, statistics, FSM and VTY remain deferred. No vendor snapshot
refresh is performed for these uncommitted changes.

Regenerate only this family (the full generator rewrites unrelated families):

```bash
./reference/run_rate_ctr_vector.sh --verify
./reference/run_rate_ctr_vector.sh --audit
./reference/run_rate_ctr_vector.sh all > /tmp/rate_ctr.1.tsv
./reference/run_rate_ctr_vector.sh all > /tmp/rate_ctr.2.tsv
./reference/run_rate_ctr_vector.sh --diagnostics > /tmp/rate_ctr.diagnostics.1.json
./reference/run_rate_ctr_vector.sh --diagnostics > /tmp/rate_ctr.diagnostics.2.json
cmp /tmp/rate_ctr.1.tsv /tmp/rate_ctr.2.tsv
cmp /tmp/rate_ctr.1.tsv tests/vectors/rate_ctr/rate_ctr_vectors.tsv
cmp /tmp/rate_ctr.diagnostics.1.json /tmp/rate_ctr.diagnostics.2.json
cmp /tmp/rate_ctr.diagnostics.1.json tests/vectors/rate_ctr/rate_ctr_diagnostics.json
cmp tests/support/rate_ctr_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/rate_ctr_fixture.rs
cargo fmt --check
cargo test
```

The source and generator manifests are copied unchanged from `reference/`.
If an intentional evidence change is accepted, refresh their hashes and the
shared reader pins in both repositories; inspect the complete fixture diff.
Expected output must come from C. The full generator stages successful and
diagnostic output before publishing either. ASan+UBSan audit and both pairs of
fresh generations passed for Stage 5a; LSan is disabled under ptrace. Actual
Rust/Cargo 1.97.1 was used, and Rust 1.70 execution remains pending.

Stage 5a final checks passed 93 tests here and 164 in the Rust conversion
crate, with formatting/whitespace checks in both repositories. Shared evidence,
readers and manifests matched byte-for-byte. Existing time, TDef and time_cc
source verification passed; those frozen fixture families were unchanged.

## FSM execution, lifecycle and TDef integration (Stage 6)

Five dedicated direct-source runners freeze the approved evidence gates:

| Runner family | Successful records | Scenarios | Isolated probes |
| --- | ---: | ---: | ---: |
| `fsm_core` | 1,506 | 64 | 7 |
| `fsm_timer` | 9,369 | 1,080 | 5 |
| `fsm_tree` | 2,010 | 134 | 5 |
| `fsm_registry` | 1,524 | 127 | 2 |
| `fsm_shared` | 443 | 21 | 0 |

Each `run_FAMILY_vector.sh` compiles the required unmodified local FSM, TDef,
timer, utility, logging and support sources. It pins 39 project files and 149
platform dependencies, including real libtalloc, checks every transitive include,
compiler version/target and the source/generator manifests, clears search
injection variables, and uses the checked libtalloc directory at runtime.
No observed API is stubbed and no system libosmocore is substituted. Required C
revision is `unknown` because the source snapshot lacks Git metadata. Logging
setup has process lifetime, without targets. Successful domains pass ASan+UBSan;
LSan is disabled under ptrace. Unsafe cases are bounded isolated subprocesses.

6a covers only standalone execution and termination without parent/children.
6b freezes callback-time state/timer mutations and failed transitions with active
timers for all five forms, timeout return values and the TDef bridge. 6c captures
lookup visibility, hierarchy membership, cleanup and parent-notification ordering,
including safe/legacy termination and nested cleanup. Four demonstrated hierarchy
UAF cases are separate diagnostics. 6d records ID/name lookup, ASCII identifier
validation/sanitization, rejected-update preservation, duplicate order, slot reuse,
nullable state names, unregistration and allocation afterward. Shared-scheduler
records cover simultaneous FSM/application timers, peer mutation, callback-created
timers and TDef configuration changes. Source/generator digest changes from pinning
the runtime library directory changed only metadata in core/timer/tree observations;
the later registry refinement changed one scenario's state-name observations from
"one" to null. Both were reviewed before freezing and replay.

The Rust implementation intentionally owns its registry instead of reproducing C
process-global ownership. C-named wrappers take that runtime. C actually allows
allocation after unregistering despite its comment; the Rust implementation follows
the evidence. Storage retention and lookup visibility are separate, with checked
handles/tombstones during safe termination cascades. Logging/VTY/CTRL, native address
names, arbitrary talloc ownership/destructors, select/timerfd and statistics remain
outside this conversion. No vendor snapshot refresh is performed. Scope and safety
boundaries are documented in the sibling's `osmocore-rs/src/fsm/README.md`.

Each family's dedicated `tests/vectors/FAMILY/` directory contains TSV records,
separate diagnostic JSON and source/generator manifests. The shared independent
`tests/support/fsm_fixture.rs` validates chronology, canonical numbers/hex strings,
record shape, frozen observations/manifests and corruption rejection. It contains
no Rust FSM conversion model. Rust behavior replay belongs in the sibling project.
Core/timer records contain scenario, step, operation, argument, status and instance
state/T/pending/deadline/terminating/ID lookup/name lookup/count; timer records add
four operation inputs and the wall-clock value. Tree/shared records contain scenario,
step, operation, target, argument and status, then four complete instance snapshots
(state, T, terminating, visibility, parent index, children mask, pending, deadline
seconds/useconds) and instance list order. Registry records contain query/name helper
inputs/outputs, registration/list order and three state/T/visibility/ID/name snapshots.
`-1` marks absent instance/index; `-2` marks an unavailable pointer identity or an
ID lookup intentionally skipped because C would dereference a null ID. `s:` encodes
lowercase hexadecimal string bytes and `s:-` is null. Each file starts with the
ABI and provenance metadata line.

Targeted verification/regeneration, without rewriting unrelated fixture families:

```bash
for family in fsm_core fsm_timer fsm_tree fsm_registry fsm_shared; do
    runner="./reference/run_${family}_vector.sh"
    "$runner" --verify
    "$runner" --audit
    "$runner" all > "/tmp/${family}.1.tsv"
    "$runner" all > "/tmp/${family}.2.tsv"
    "$runner" --diagnostics > "/tmp/${family}.diagnostics.1.json"
    "$runner" --diagnostics > "/tmp/${family}.diagnostics.2.json"
    cmp "/tmp/${family}.1.tsv" "/tmp/${family}.2.tsv"
    cmp "/tmp/${family}.1.tsv" "tests/vectors/${family}/${family}_vectors.tsv"
    cmp "/tmp/${family}.diagnostics.1.json" "/tmp/${family}.diagnostics.2.json"
    cmp "/tmp/${family}.diagnostics.1.json" "tests/vectors/${family}/${family}_diagnostics.json"
done
cmp tests/support/fsm_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/fsm_fixture.rs
cargo fmt --check
cargo test
```

Source and generator manifests in every fixture directory must match `reference/`
and the sibling's dedicated fixture directory byte-for-byte. Accepted evidence
changes require updating both shared-reader test specs, inspecting record differences,
repeating generation and auditing. Expected results come exclusively from C.
The full generator stages successful/diagnostic outputs before publishing either
and copies the verified manifests. Stage 6 passed all five audits and repeat
comparisons, 98 tests here and 178 in the conversion crate. Actual Rust/Cargo 1.97.1
was used; Rust 1.70 execution remains pending. Prior time/TDef/time_cc/rate-counter
fixtures remain unchanged.

## Stage 7 rate-counter groups and rollups

`rate_ctr_group_vector.c` includes the required unmodified local `rate_ctr.c`
for direct access to its static callback/tick input. It calls the actual group,
registry, lookup, reset and iteration APIs and delivers expiration counts via
pipe reads. Native initialization probes use real select/timerfd source with
OS-boundary failure injection, run in separate processes; a bounded real
one-second select dispatch smoke check also runs. These characterize C wiring,
not a Rust native timerfd/select API-shape parity claim.

The closed runner pins 47 local source/header files and 227 platform
headers/library files, compiler identity, exact normal/audit/link flags and
all preprocessing defines. `rate_ctr_group_preprocessor_defines.txt` is pinned
in the generator manifest and recomputed during source verification. Build
sources include stats-TCP/stat-item/socket/io_uring dependencies of select;
no conversion of those families is implied. All observed source paths are
sanitizer-instrumented during audit; pinned external system libraries are not.
LSan is disabled under ptrace, following the established runner convention.

The dedicated `tests/vectors/rate_ctr_group/` family has 1,915 complete-state
records across 608 scenarios. It preserves group order, effective descriptor
metadata, numeric indices/classes, optional report names, callback trace/return,
tick phase and all current/previous/interval words before and after operations.
It contains full group/counter identifier byte matrices; duplicate-index
wrapping/mangling; lifecycle and lookup; counter reset during iteration; nested
lookup/iteration; current/next group mutation, removal and allocation; interval
boundaries, tick/counter wrap and delayed catch-up. Four records directly
characterize valid/mangled descriptor aliasing and copied report names. The
Rust crate replays 1,911 selected-behavior records and documents deep-copy
ownership differences for the four aliasing records.

Native initialization has a separate three-row TSV. Seven isolated diagnostics
cover continuing after freeing/unlinking the current group, out-of-bounds
counter access, null descriptor, short expiration read, zero expiration count,
and reinitializing the current unlinked list node (bounded nontermination).
Freeing the next group and freeing the current group with an immediate negative
callback return are in the successful domain. Native failure/unsafe probes use
independent processes; successful scenarios explicitly reset group/tick inputs.
The standalone Stage 5a fixture remains unchanged.

Regenerate only this family while implementing Stage 7:

```bash
./reference/run_rate_ctr_group_vector.sh --verify
./reference/run_rate_ctr_group_vector.sh --audit
./reference/run_rate_ctr_group_vector.sh all > /tmp/rate_ctr_group.1.tsv
./reference/run_rate_ctr_group_vector.sh all > /tmp/rate_ctr_group.2.tsv
./reference/run_rate_ctr_group_vector.sh --diagnostics > /tmp/rate_ctr_group.diagnostics.1.json
./reference/run_rate_ctr_group_vector.sh --diagnostics > /tmp/rate_ctr_group.diagnostics.2.json
./reference/run_rate_ctr_group_vector.sh --native > /tmp/rate_ctr_group.native.1.tsv
./reference/run_rate_ctr_group_vector.sh --native > /tmp/rate_ctr_group.native.2.tsv
cmp /tmp/rate_ctr_group.1.tsv /tmp/rate_ctr_group.2.tsv
cmp /tmp/rate_ctr_group.1.tsv tests/vectors/rate_ctr_group/rate_ctr_group_vectors.tsv
cmp /tmp/rate_ctr_group.diagnostics.1.json /tmp/rate_ctr_group.diagnostics.2.json
cmp /tmp/rate_ctr_group.diagnostics.1.json tests/vectors/rate_ctr_group/rate_ctr_group_diagnostics.json
cmp /tmp/rate_ctr_group.native.1.tsv /tmp/rate_ctr_group.native.2.tsv
cmp /tmp/rate_ctr_group.native.1.tsv tests/vectors/rate_ctr_group/rate_ctr_group_native.tsv
cmp tests/support/rate_ctr_group_fixture.rs ../libosmocore-rs/osmocore-rs/tests/support/rate_ctr_group_fixture.rs
```

Review every changed observation. Expected results come exclusively from C.
The independent shared reader pins corpus/manifests/diagnostics/native records,
canonical encoding, chronology, full retained state, and an input/coverage
fingerprint. Copy all five fixture artifacts byte-for-byte to the Rust crate's
`tests/compatibility/vectors/rate_ctr_group/` directory. Ordinary tests do not
require a C installation, and fixtures never enter packet/replay loaders.

For an intentional harness change, update the generator manifest hashes of the
C harness, runner and macro dump. For a reference/build-input change, first
review exact source/header/platform hashes, compiler identity and the explicit
flags. Recompute the macro dump with the pinned flags (`cc -dM -E` on the
harness), update its digest and the reference manifest, then update the
reference-manifest digest in generator provenance. Never borrow a parent Git
revision: the required snapshot remains `unknown`. Regenerate/review all
Stage 7 artifacts, update frozen reader fingerprints/input counts as applicable,
audit and repeat all generations before accepting new evidence.

Stage 7 ends the planned initial upstream conversion. Productization/release
work follows; no vendor snapshot refresh, commit, push or publication is part
of this evidence update.

Stage 7 acceptance on 2026-10-12 passed all 100 testing-utility tests, including
localhost tests with sandbox escalation, and 182 conversion-crate tests plus one
public-API documentation example. Both formatting/whitespace checks passed.
The new C source/flags/defines verification and ASan+UBSan audit passed; two
independent successful, diagnostic and native generations matched frozen
fixtures, all shared artifact/reader bytes matched, and existing Stage 1–6
source verification passed. The full generator was syntax-checked; only Stage 7
commands generated observations. Actual Rust/Cargo was 1.97.1, not the declared
MSRV or CI toolchain. No Stage 5a evidence, vendor snapshot, C reference source,
commit, push or publication was changed by this stage.
