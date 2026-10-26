# CRC reference

`crc_vector.c` calls the byte-oriented CRC-16/CCITT API and the generic
parameterized CRC API for 8, 16, 32 and 64-bit storage. GSM parity and channel
definitions are not covered; convolutional coding has its own harness in
[`conv/`](../conv/README.md).

## Fixtures

| Mode | Fixture in `tests/vectors/crc/` | Records |
| --- | --- | ---: |
| `byte` (default) | `crc_byte_vectors.json` | 3,472 |
| `generic` | `crc_generic_vectors.json` | 22,848 |

- **Byte CRC:** all 512 table entries, 2,560 single-byte calls covering every
  byte with five initial states, and 400 buffer scenarios. Each scenario calls
  the whole-buffer, first-chunk and continuation APIs and records all 260 input
  bytes before and after, including an untouched suffix. Zero-length calls pass
  a null input pointer.
- **Generic CRC:** every configured width for each storage type, four neutral
  descriptors, four input patterns including nonbinary bytes, lengths
  `INT_MIN`, −1, 0, 1, 7 and 32, and exact and oversized destinations. Each
  record observes compute, set, a matching check, a single-bit mismatch at
  every position and a nonbinary CRC byte at every position. Input and
  destination bytes are recorded before and after, with canaries after the
  destination.

`tests/support/crc_fixture.rs` checks exact `u64` and `i32` values, schema,
provenance, ordered coverage and complete output state, and pins an FNV-1a
fingerprint of each file.

## Excluded cases

For positive input lengths, the full-width 32 and 64-bit masks shift by the
whole storage width, which is undefined. Those calls are not golden cases;
their zero and negative-length calls are defined and included. Full-width
CRC-8 and CRC-16 are defined through integer promotion and are included for all
lengths.

`--audit` builds the exact local CRC sources with UBSan. It requires the whole
golden domain to be clean, and requires diagnostics for the full-width 32 and
64-bit shifts, zero width and the `INT_MIN` width subtraction. The diagnostics
are never golden results.

## Provenance

The runner checks the hashes of 16 files: `crc16.c` and `crc16.h`, the
`crcXXgen.c.tpl` and `crcXXgen.h.tpl` templates, `crcgen.h`, `bits.h`, both
`Makefile.am` generation rules and the eight generated `crc{8,16,32,64}gen`
sources and headers. The nongenerated files were verified byte-for-byte against
upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311`; the generated
files match the templates' `XX` substitutions. The recorded ABI is 8-bit bytes
and 32-bit `int`.

## Checking

```bash
./reference/crc/run_crc_vector.sh byte | cmp - tests/vectors/crc/crc_byte_vectors.json
./reference/crc/run_crc_vector.sh generic | cmp - tests/vectors/crc/crc_generic_vectors.json
./reference/crc/run_crc_vector.sh --audit
```
