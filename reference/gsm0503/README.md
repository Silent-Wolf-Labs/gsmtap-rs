# GSM 05.03 channel coding reference

Three harnesses call the public `libosmocoding`, `libosmocodec` and `libosmogsm` APIs
directly. Each writes NDJSON: a metadata line, followed by one record per descriptor or
call. Every destination byte starts as `0xa5`, and four guard bytes follow the requested
length. Soft bits stay within the documented `sbit_t` range of -127 to 127.

## xCCH

`xcch_vector.c` records:

- the `gsm0503_xcch` convolutional code and `gsm0503_fire_crc40` parity descriptors;
- `gsm0503_xcch_interleave()` and `gsm0503_xcch_deinterleave()` on inputs that reveal
  the whole permutation;
- `gsm0503_xcch_burst_map()`, with and without each stealing flag, and
  `gsm0503_xcch_burst_unmap()`;
- `gsm0503_xcch_encode()`, including every single-bit frame;
- `gsm0503_xcch_decode()` on upstream's frames, clean frames at several amplitudes,
  erased bursts, bit flips, additive noise, random erasures, altered stealing flags
  and input with no signal.

It replays the three xCCH frames in `tests/coding/coding_test.ok` and fails if the
bursts, frame, error count or bit total differ from that file.

`tests/vectors/gsm0503/xcch_vectors.ndjson` is read by
`tests/support/gsm0503_fixture.rs`, which pins an FNV-1a fingerprint of the file and
checks the sections, fields, provenance and buffer contract: encoded output is
unpacked bits with both stealing flags set, guard bytes stay untouched and a failed
decode leaves its destination alone.

## Convolutional codes and parity

`codes_vector.c` records, for each of the 45 `gsm0503_*` codes declared in
`osmocom/gsm/gsm0503.h`:

- the whole descriptor: rate, constraint length, frame length, termination, the
  output and state tables, the termination tables of the recursive codes and the
  puncture positions, plus `osmo_conv_get_input_length()` and
  `osmo_conv_get_output_length()`;
- `osmo_conv_encode()` on all-zero, all-one and alternating frames, the first and last
  single-bit frames and random frames;
- `osmo_conv_decode()` on clean frames at amplitudes 127 and 1, bit flips, additive
  noise, random erasures, an erased first quarter, all-zero input and random soft
  bits.

It also records the ten `gsm0503_parity` CRC descriptors, and for each one
`compute_bits`, `set_bits` and `check_bits` on random, all-zero and all-one inputs,
with the check repeated after flipping the first parity bit.

Upstream's `tests/conv/conv_gsm0503_test.ok` has no precomputed vectors. Its round
trips use random input seeded from the clock, so only the lengths it prints are
reproducible. The harness requires that file to name the same 45 codes in the same
order, with the same input and output lengths.

`osmo_conv_decode()` returns 0 from the correlation decoder, which takes every code
with N ≤ 4. `gsm0503_tch_f24` (N = 6) and `gsm0503_tch_afs_5_15` and
`gsm0503_tch_afs_4_75` (N = 5) go to the generic decoder, which returns its path
metric.

`tests/vectors/gsm0503/codes_vectors.ndjson` is read by
`tests/support/gsm0503_codes_fixture.rs`. It pins an FNV-1a fingerprint and upstream's
lengths, and checks the record order, fields, table shapes and buffer contract: outputs
are unpacked bits, guard bytes stay untouched, every parity check passes and every
flipped one fails.

## TCH/FS, TCH/EFS and FACCH/F

`tch_fr_vector.c` records:

- the `gsm610_bitorder` and `gsm660_bitorder` tables from `libosmocodec`, and
  `gsm0503_gsm_fr_map` and `gsm0503_gsm_efr_protected_bits`;
- `gsm0503_tch_fr_interleave()` and `gsm0503_tch_fr_deinterleave()` on inputs that
  reveal the whole permutation;
- `gsm0503_tch_burst_map()` for even and odd bursts, with no stealing flag and with
  each value, and `gsm0503_tch_burst_unmap()`;
- `gsm0503_tch_fr_encode()` on every single-bit FR frame in both bit orders and every
  single-bit EFR frame, random FR, EFR and FACCH frames, the zero-length bad-frame
  request and invalid lengths;
- `gsm0503_tch_fr_decode()` on FR (both bit orders), EFR, FACCH and bad-frame blocks:
  clean at several amplitudes, decoded with the other settings, erased bursts, bit
  flips, additive noise, random erasures, altered stealing flags, and input with no
  signal;
- `gsm0503_tch_fr_facch_encode()` and `gsm0503_tch_fr_facch_decode()`, including flag
  sums at and just above the FACCH decision threshold and speech blocks.

TCH/F interleaving spreads a block over eight bursts, using the even data bits of the
first four and the odd data bits of the last four. The encoders write only those
positions and the stealing flag of each burst, leaving the rest to the neighbouring
blocks. The harness starts every destination as `0xa5`, so the fixture shows the
untouched positions. When it decodes, it fills them with random soft bits.

The decoder takes a block as FACCH when the sum of `hu(B)` in the first four bursts
and `hl(B)` in the last four is negative. A speech block's bit errors cover only the
378 convolutionally coded bits, not the 78 unprotected class 2 bits. FR and EFR share
the 3-bit CRC, so an EFR block decoded as FR passes. The zero-length request encodes
all-zero bits with all-zero parity, which never decodes.

The harness replays the five `test_fr` frames (FR, EFR and three FACCH blocks) and
the three FACCH/F `test_facch` frames in `tests/coding/coding_test.ok`. It fails if the
bursts, frame, error count or bit total differ from that file. Like upstream, the
`test_facch` replay encodes into zeroed bursts.

`tests/vectors/gsm0503/tch_fr_vectors.ndjson` is read by
`tests/support/gsm0503_tch_fr_fixture.rs`. It pins an FNV-1a fingerprint and checks
the record order, fields, tables and buffer contract: an encoder writes exactly one
block's positions with bits or, on error, nothing, a decoder writes only the frame it
returns, and decode results agree with their bit totals and settings.

## Provenance

Both runners check the hashes of the sources and headers they use. The xCCH runner also
checks `tests/coding/coding_test.c` and `tests/coding/coding_test.ok`. The codes runner
also checks `tests/conv/conv_gsm0503_test.c`, `tests/conv/conv.c`, `tests/conv/conv.h`
and `tests/conv/conv_gsm0503_test.ok`. The TCH/FR runner also checks `gsm0503_tables.c`
and its header, `src/codec/gsm610.c`, `src/codec/gsm660.c`,
`include/osmocom/codec/codec.h` and both `coding_test` files.

All are verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`, except the files the build generates, which
upstream doesn't track: `src/gsm/gsm0503_conv.c`, `include/osmocom/gsm/gsm0503.h` and
the `crc8gen`, `crc16gen` and `crc64gen` sources and headers. For those, the generators
`utils/conv_gen.py`, `utils/conv_codes_gsm.py`, `src/core/crcXXgen.c.tpl` and
`include/osmocom/core/crcXXgen.h.tpl` are verified instead.

## Decoder backends

C selects a scalar, SSE or AVX correlation decoder by CPU at run time. To check that
the fixtures don't depend on that, run:

```bash
./reference/gsm0503/run_xcch_vector.sh --check-backends
./reference/gsm0503/run_codes_vector.sh --check-backends
./reference/gsm0503/run_tch_fr_vector.sh --check-backends
```

Each builds its harness once per backend from the pinned sources, confirms that the
forced decoder replaces the library's, and compares the complete output with the linked
library's. All three are identical for every fixture. It needs an x86-64 CPU with AVX2.

## Checking

```bash
./reference/gsm0503/run_xcch_vector.sh | cmp - tests/vectors/gsm0503/xcch_vectors.ndjson
./reference/gsm0503/run_codes_vector.sh | cmp - tests/vectors/gsm0503/codes_vectors.ndjson
./reference/gsm0503/run_tch_fr_vector.sh | cmp - tests/vectors/gsm0503/tch_fr_vectors.ndjson
```
