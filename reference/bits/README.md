# Bit-packing reference

Two harnesses record conversion between packed bits and unpacked one-byte-per-bit
arrays:

- `bit_packing_vector.c` calls `osmo_ubit2pbit()` and `osmo_pbit2ubit()`.
- `bit_packing_ext_vector.c` calls `osmo_ubit2pbit_ext()` and
  `osmo_pbit2ubit_ext()`, which add bit offsets and an LSB-first mode.

Soft-bit conversion is not covered.

## Fixtures

| File | Records |
| --- | ---: |
| `tests/vectors/bits/bit_packing_vectors.json` | 9,016 |
| `tests/vectors/bits/bit_packing_ext_vectors.json` | 63,384 |

Sources and before/after destinations are recorded in full, and the C backing
arrays are allocated at exactly the recorded lengths. Oversized destinations add
three bytes. Case names, inputs, order and counts are frozen in
`tests/bit_packing_reference_vectors.rs` and
`tests/bit_packing_ext_reference_vectors.rs`.

### Basic packing

Every record has exactly `case`, `api`, `libosmocore_commit`, `num_bits`,
`src_hex`, `dst_len`, `dst_before_hex`, `return_code` and `dst_after_hex`.
Destinations start as `0xa5 ^ index`. The cases, in order:

- 8,192 exhaustive cases: every byte `00` to `ff`, lengths 1 to 8, unpack then
  pack, exact then oversized destinations.
- 780 boundary cases: lengths 7, 8, 9, 15, 16, 17, 31, 32 and 33, with all-zero,
  all-one, alternating and every single-set-bit pattern.
- 36 extra-source cases: identical requested input with contrasting
  three-byte source suffixes.
- 8 zero-bit cases.

### Extended packing

Each record has `case`, `api`, `libosmocore_commit`, `c_unsigned_bits`,
`out_ofs`, `in_ofs`, `num_bits`, `lsb_mode`, `src_hex`, `dst_len`,
`dst_before_hex`, `return_code` and `dst_after_hex`, in that order.

| Family | Cases | Coverage |
| --- | ---: | --- |
| `exhaustive` | 36,864 | Every byte, offset 0–7, count 1 to 8−offset, both APIs, modes 0 and 1 |
| `boundary` | 19,104 | Output offset, equal or next input offset, count, pattern, API and mode |
| `offsets` | 5,760 | Every independent output and input offset pair |
| `truthy` | 1,296 | Nonbinary source values `02`, `80` and `ff`; modes 0, 1, 2 and −1 |
| `neighbors` | 192 | Zero and all-one bytes around the requested source range |
| `zero` | 168 | Zero counts across modes and offsets |

The offset list is `0,1,2,3,4,5,6,7,8,9,15,16` and the counts are
`1,7,8,9,15,16,17,31,32,33`. Destination initialization cycles through `00`,
`ff` and `a5 ^ index`.

## Recorded C behavior

These results come from the C calls and must not be normalized:

- Basic packing returns the packed byte count, clears unused low bits in a
  partial final byte and preserves the destination suffix. Unpacking writes
  only 0 or 1 and returns the bit count.
- With zero bits, basic packing returns 0 and writes nothing, but basic
  unpacking reads `src[0]`, writes its top bit to `dst[0]` and returns 1.
- Extended packing treats any nonzero source byte as one. Mode 0 is MSB first;
  any nonzero mode is LSB first.
- Extended calls return the ending output position, not the number of bits
  written: `ceil((out_ofs + num_bits) / 8)` for packing and
  `out_ofs + num_bits` for unpacking.
- With zero bits, the extended calls read and write nothing. Unpacking returns
  `out_ofs`. Packing returns `ceil(out_ofs / 8)`, except at output offset 0,
  where an unsigned subtraction underflows to 536870912 on the recorded 32-bit
  `unsigned int` ABI.

## Excluded cases

Undersized buffers, NULL pointers and offset arithmetic that would overflow are
unsafe in C and are never cases. Nonbinary unpacked input to basic packing is
outside the basic fixture's domain.

## Provenance

Both runners check the hashes of `src/core/bits.c`
(`bb33ceaf…c9fa1d`) and `include/osmocom/core/bits.h` (`922f568b…c297f7`),
verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The extended runner also requires
8-bit bytes and 32-bit `int` and `unsigned int`.

## Checking

```bash
./reference/bits/run_bit_packing_vector.sh | cmp - tests/vectors/bits/bit_packing_vectors.json
./reference/bits/run_bit_packing_ext_vector.sh | cmp - tests/vectors/bits/bit_packing_ext_vectors.json
```
