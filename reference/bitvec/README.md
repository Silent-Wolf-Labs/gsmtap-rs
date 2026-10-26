# Bit-vector reference

Four harnesses call the `bitvec_*` API directly. `run_bitvec_vector.sh`
selects one with a mode flag. Every call records return codes, cursor state and
the complete vector backing, including bytes beyond the declared vector length.

## Fixtures

| Mode | Harness | Fixture in `tests/vectors/bitvec/` | Records |
| --- | --- | --- | ---: |
| (none) | `bitvec_vector.c` | `bitvec_vectors.json` | 5,944 |
| `--buffers` | `bitvec_buffers_vector.c` | `bitvec_buffers_vectors.json` | 38,837 |
| `--operations` | `bitvec_operations_vector.c` | `bitvec_operations_vectors.json` | 38,643 |
| `--adapters` | `bitvec_adapters_vector.c` | `bitvec_adapters_vectors.json` | 6,757 |

Each fixture has its own reader in `tests/support/bitvec_*_fixture.rs`, built on
the shared parser `bitvec_fixture.rs`, and a test in
`tests/bitvec_*reference_vectors.rs`.

## Coverage

- **Primitives:** positional binary and L/H reads, all four bit values for
  positional and sequential writes, sequential L/H reads, bulk writes
  including partial failures, zeroing, used-byte and tailroom helpers and
  bit-value characters. Lengths 0 to 3, three byte patterns, every in-range
  position and invalid positions up to `UINT_MAX`.
- **Integers and buffers:** `bitvec_set_u64`, `bitvec_set_uint`,
  `bitvec_get_uint`, `bitvec_get_int16_msb`, `bitvec_fill`,
  `bitvec_spare_padding`, `bitvec_get_bytes` and `bitvec_set_bytes`. Lengths 0,
  1, 2, 4 and 8, widths 0 to 64 and beyond, and all eight byte alignments.
  Records include six transfer bytes, capacity failures, partial progress,
  signed 32-bit results, count narrowing and wrapping fill arithmetic.
- **Indexed operations:** indexed field reads and writes, nth-set-bit and
  binary searches, both run-length helpers, left shifts and CSN.1 array
  encoding and estimates. Records include 64-bit results, `errno` and external
  index state.
- **Adapters:** full-buffer pack and unpack, hex loading and string
  formatting. Hex inputs include malformed, odd-length, embedded-NUL,
  whitespace, high-byte and every single byte. Formatting covers every valid
  cursor through eight bytes over eight patterns.

## Recorded C behavior

- Tailroom is computed in 32-bit unsigned arithmetic and wraps.
- Shifts keep the C routine's retained tail and skipped penultimate byte.
- Array encoding covers dry-run wraparound, ignored setter errors and partial
  writes.
- The formatting harness allocates `cur_bit + ceil(cur_bit / 8) + 1` bytes,
  which includes the separator spaces the C comment leaves out.

## Excluded cases

Undefined shifts, scalar reads beyond the declared vector length, short
buffers and invalid enum values are never cases. Talloc-allocated vectors are
not recorded; the harnesses use caller-owned storage.

## Provenance

The runner checks the hashes of `src/core/bitvec.c`,
`include/osmocom/core/bitvec.h` and `tests/bitvec/bitvec_test.c`. For
`--adapters` it also checks `src/core/utils.c`, `include/osmocom/core/utils.h`
and `tests/utils/utils_test.c`, which hold the hex parser. All were verified
byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. It requires a 32-bit `unsigned int`
and, for the later modes, 32-bit signed returns and the recorded `EINTR` and
`EOVERFLOW` values.

## Checking

```bash
./reference/bitvec/run_bitvec_vector.sh | cmp - tests/vectors/bitvec/bitvec_vectors.json
./reference/bitvec/run_bitvec_vector.sh --buffers | cmp - tests/vectors/bitvec/bitvec_buffers_vectors.json
./reference/bitvec/run_bitvec_vector.sh --operations | cmp - tests/vectors/bitvec/bitvec_operations_vectors.json
./reference/bitvec/run_bitvec_vector.sh --adapters | cmp - tests/vectors/bitvec/bitvec_adapters_vectors.json
```
