# GSM 05.03 xCCH reference

`xcch_vector.c` calls the public `libosmocoding` API directly and records:

- the `gsm0503_xcch` convolutional code and `gsm0503_fire_crc40` parity descriptors;
- `gsm0503_xcch_interleave()` and `gsm0503_xcch_deinterleave()` on inputs that reveal
  the whole permutation;
- `gsm0503_xcch_burst_map()`, with and without each stealing flag, and
  `gsm0503_xcch_burst_unmap()`;
- `gsm0503_xcch_encode()`, including every single-bit frame;
- `gsm0503_xcch_decode()` on upstream's frames, clean frames at several amplitudes,
  erased bursts, bit flips, additive noise, random erasures, altered stealing flags
  and input with no signal.

Every destination byte starts as `0xa5`, and four guard bytes follow the requested
length. Soft bits stay within the documented `sbit_t` range of -127 to 127.

## Fixtures

`tests/vectors/gsm0503/xcch_vectors.ndjson` starts with a metadata line, followed
by one record per call. `tests/support/gsm0503_fixture.rs` pins an FNV-1a fingerprint
of the file and checks the sections, fields, provenance and buffer contract: encoded
output is unpacked bits with both stealing flags set, guard bytes stay untouched and
a failed decode leaves its destination alone.

## Provenance

The runner checks the hashes of the coding, interleaving, mapping, parity and
convolutional sources and headers, and of `tests/coding/coding_test.c` and
`tests/coding/coding_test.ok`. All are verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`, except `src/gsm/gsm0503_conv.c` and
`src/core/crc64gen.c`, which upstream doesn't track because the build generates them.
For those two, the generators `utils/conv_gen.py`, `utils/conv_codes_gsm.py` and
`src/core/crcXXgen.c.tpl` are verified instead.

The harness replays the three xCCH frames in `coding_test.ok` and fails if the bursts,
frame, error count or bit total differ from that file.

## Decoder backends

C selects a scalar, SSE or AVX decoder by CPU at run time. To check that the fixture
doesn't depend on that, run:

```bash
./reference/gsm0503/run_xcch_vector.sh --check-backends
```

This builds the harness once per backend from the pinned sources, confirms that the
library calls the forced decoder, and compares the complete output with the linked
library's. All three are identical. It needs an x86-64 CPU with AVX2.

## Checking

```bash
./reference/gsm0503/run_xcch_vector.sh | cmp - tests/vectors/gsm0503/xcch_vectors.ndjson
```
