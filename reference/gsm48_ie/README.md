# 3GPP TS 24.008 BCD number reference

`gsm48_ie_vector.c` calls libosmogsm's BCD number functions from `gsm48_ie.h` directly.
They carry the called, calling and connected party numbers (3GPP TS 24.008 section
10.5.4.7). It writes one section per kind of record:

- `encode` for `gsm48_encode_bcd_number()` on every digit and filler character, invalid
  characters in each position, and numbers up to 21 digits, with headers of 0, 1, 2 and 4
  bytes and limits of 0, one under the length byte, the length byte, one over it and 255.
  Numbers of 506 to 513 and 600 digits reach and pass a length byte of 255;
- `decode` for `gsm48_decode_bcd_number2()` on every byte as the only digit byte, numbers
  with the filler in each position, every output length from 0 to one past the number,
  header lengths up to past the IE, input lengths up to one past the IE, and the longest
  IE of 255 bytes;
- `decode_legacy` for `gsm48_decode_bcd_number()` on each of those inputs whose length byte
  stays within it.

Outputs are written into buffers of `0x55`. An encode record holds every byte the call may
write and one more; a decode record holds the output up to its length, but no further than
two digits for each input byte and a NUL, and one more. The harness fails if a call writes
past either.

## Recorded C behavior

- **Digits:** the nibbles 0 to 9 are digits, A to E are `*`, `#`, `a`, `b` and `c`, and F
  is the filler. The encoder takes only those characters, in lowercase.
- **Encoding:** `gsm48_encode_bcd_number()` writes the length byte first, then the digits
  after `h_len` header bytes, which it leaves alone. It returns the bytes written. A length
  byte past `max_len` returns `-EIO` with only that byte written, and an invalid character
  returns `-EINVAL` after the digits before it, a low nibble alone included.
- **Length limit:** `max_len` bounds the length byte, not the buffer, so a call that
  succeeds writes up to `max_len + 1` bytes. The length byte is 8 bits and wraps: 511
  digits give 0 and write 257 bytes, and 600 give `0x2c` and write 301.
- **Decoding:** `gsm48_decode_bcd_number2()` turns the filler into a NUL wherever it is, so
  a filler before the last nibble ends the text early, though the digits after it are
  still written. A header past the IE gives an empty number. It returns `-EIO` for no
  input, `-EINVAL` when the length byte passes the input, and writes only a NUL for either.
  Input past the IE is ignored.
- **Short output:** the decoder writes as many digits as fit before a NUL and returns
  `-ENOSPC`, unless the only nibble left is a final filler. An output length of 0 writes
  nothing and returns `-ENOSPC`.
- **Legacy decoding:** `gsm48_decode_bcd_number()` takes the length byte as the input
  length, and so gives the same results.

## Fixtures

`tests/vectors/gsm48_ie/gsm48_ie_vectors.ndjson` starts with a metadata line, followed by
one section per kind of record. `tests/support/gsm48_ie_fixture.rs` pins an FNV-1a
fingerprint of the file and checks the sections, fields, provenance, result codes, that
the encoder wrote its length byte and left the header alone, and that outputs end where
they say.

## Provenance

The runner checks the hashes of `src/gsm/gsm48_ie.c`, `include/osmocom/gsm/gsm48_ie.h`,
`src/core/utils.c` (for `osmo_hexparse()` and `osmo_hexdump_nospc()`),
`include/osmocom/core/utils.h`, `tests/gsm0408/gsm0408_test.c` and
`tests/gsm0408/gsm0408_test.ok`. Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The runner builds with
`-Wno-deprecated-declarations`, since `gsm48_decode_bcd_number()` is declared deprecated.

The harness replays upstream's `test_bcd_number_encode_decode` with the same calls, and
records them with the source `upstream`. It fails unless the output it prints matches the
part of `gsm0408_test.ok` that test prints, byte for byte.

## Checking

```bash
./reference/gsm48_ie/run_gsm48_ie_vector.sh | cmp - tests/vectors/gsm48_ie/gsm48_ie_vectors.ndjson
```
