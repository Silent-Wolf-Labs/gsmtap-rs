# 3GPP TS 24.008 identity encoding reference

`gsm48_vector.c` calls libosmogsm's identity encodings directly, from `gsm48.h` and the
PLMN BCD functions of `gsm23003.h`. It records the `gsm_04_08.h` and `gsm_23_003.h`
constants they use, then one section per kind of record:

- `to_bcd` for `osmo_plmn_to_bcd()` and `gsm48_mcc_mnc_to_bcd()` on MCC and MNC values from
  0 to 65535, with both MNC flags;
- `from_bcd` for `osmo_plmn_from_bcd()`, `gsm48_mcc_mnc_from_bcd()` and the PLMN of
  `gsm48_parse_ra()` on every combination of the nibbles 0, 9, A and F, and on every value
  of each byte with the others valid;
- `lai_encode` and `lai_decode` for `gsm48_generate_lai2()`, `gsm48_generate_lai()`,
  `gsm48_decode_lai2()` and `gsm48_decode_lai()`;
- `ra_encode` for `osmo_routing_area_id_encode_buf()` with every buffer length up to two
  past the encoding, and for `gsm48_encode_ra()` and `gsm48_construct_ra()` on the same
  values. The harness checks that `osmo_rai_to_gprs()` and `gprs_rai_to_osmo()` copy every
  field;
- `ra_decode` for `osmo_routing_area_id_decode()` with every input length up to one past the
  encoding, and `gsm48_parse_ra()`;
- `ra_equal` for `gsm48_ra_equal()` over pairs that differ in each field;
- `rai_name` and `rai_name_buf` for `osmo_rai_name()` and its variants, and for
  `osmo_rai_name_buf()` with every buffer length up to one past the name;
- `mi_type_name` for `gsm48_mi_type_name()` on every byte;
- `mi_bytes` for encoded identities of every type nibble, with and without the odd flag,
  several first digits, lengths from 0 to 20 bytes, and a filler or a hex digit, each
  through `osmo_mi_name()` and its variants, `gsm48_mi_to_string()` and
  `osmo_mobile_identity_decode()` without and with hex digits. A NULL pointer is included;
- `mi_to_string` and `mi_name_buf` for `gsm48_mi_to_string()` and `osmo_mi_name_buf()` with
  every buffer length up to one past the text;
- `generate_mid` and `generate_mid_from_tmsi` for `gsm48_generate_mid()`,
  `gsm48_generate_mid_from_imsi()` and `gsm48_generate_mid_from_tmsi()`, on digit strings
  of every length up to 17, with hex and other characters, and every type;
- `mi_encode` for `osmo_mobile_identity_encoded_len()` and
  `osmo_mobile_identity_encode_buf()` on identities of every type with digit strings of
  each length class, hex and other characters and TMSIs, into short and long buffers,
  without and with hex digits. A NULL buffer and a NULL identity are included;
- `mi_str` for `osmo_mobile_identity_to_str_buf()` with short and long buffers, and
  `osmo_mobile_identity_to_str_c()`;
- `mi_cmp` for `osmo_mobile_identity_cmp()` over every pair of identities of each type,
  including NULL;
- `l3` for `osmo_mobile_identity_decode_from_l3_buf()` on every Complete Layer 3 message of
  upstream's test, and Talker Indications, a Paging Response under C's minimum length, hex
  digits, transaction identifiers and the N(SD) bits, each cut at every length, then on every first byte and message type.

Outputs are written into buffers of `0x55`, and the harness fails if a call writes past what
it returns. Decoded results start as junk, so a record shows which fields a call writes.

## Recorded C behavior

- **BCD digits:** `osmo_plmn_from_bcd()` and `gsm48_parse_ra()` weight each nibble by its
  decimal place without checking it, so hex nibbles give values past 999 (`aaaaaa` gives
  1110-1110). An F in the MNC's third digit marks a two-digit MNC, and any other value
  there sets the three-digit flag. `osmo_plmn_to_bcd()` writes the last three decimal
  digits of a value past 999.
- **MNC flag:** an MNC from 100 on encodes with three digits whatever the flag says. The
  legacy encoders always pass the flag as false, and the legacy decoders drop it.
  `gsm48_ra_equal()` compares the flag even from 100 on, unlike `osmo_rai_cmp()`.
- **Routing area:** `osmo_routing_area_id_encode_buf()` and `osmo_routing_area_id_decode()`
  return `-ENOSPC` for fewer than six bytes and touch nothing.
- **Mobile identity decoding:** `osmo_mobile_identity_decode()` checks the digit count for
  the type: 6 to 15 for an IMSI, 14 or 15 for an IMEI, without the check digit's Luhn
  check, 16 for an IMEISV, and exactly eight nibbles for a TMSI, whose first nibble must be
  F. An even count must end with an F filler. Without `allow_hex`, a hex digit fails. An
  unknown type returns `-EINVAL`, and any other failure `-EBADMSG`. On failure the identity
  has type NONE and a zero union, so its TMSI is 0, although the function's documentation
  says `GSM_RESERVED_TMSI`.
- **Complete Layer 3:** `osmo_mobile_identity_decode_from_l3_buf()` starts the identity as
  NONE with `GSM_RESERVED_TMSI`, keeps that when its own checks fail, and returns
  `-ENOTSUP` for a message that doesn't carry an identity it knows. It ignores the N(SD)
  bits of an MM message type, but not of an RR one.
- **Legacy strings:** `osmo_mi_name()` and `gsm48_mi_to_string()` don't check the length,
  filler or digits, and print a TMSI only from exactly five bytes with an F first nibble.
  `gsm48_mi_to_string()` returns the bytes written for digits, `snprintf()`'s count for a
  TMSI, and 1 for an empty string.
- **Legacy encoding:** `gsm48_generate_mid()` takes at most 16 characters, encodes any
  character other than a hex digit as 0, and masks the type to three bits.
- **Mobile identity encoding:** `osmo_mobile_identity_encode_buf()` returns `-EIO` for a NULL
  or empty buffer, `-ENOSPC` when the encoding doesn't fit, and `-ENOTSUP` for a type without
  an encoding. An invalid digit returns `-EINVAL` after the digits before it are written.
- **Comparison:** `osmo_mobile_identity_cmp()` orders by type, then TMSI or digits. For
  digits it returns `strncmp()`'s result, which glibc gives as the difference of the first
  differing bytes, although the documentation says -1, 0 or 1.

## Fixtures

`tests/vectors/gsm48/gsm48_vectors.ndjson` starts with a metadata line, followed by the
constants and one section per kind of record. `tests/support/gsm48_fixture.rs` pins an
FNV-1a fingerprint of the file and checks the sections, fields, provenance, result codes,
that failed calls left their results untouched and that outputs end where they say.

## Provenance

The runner checks the hashes of `src/gsm/gsm48.c`, `src/gsm/gsm23003.c`,
`include/osmocom/gsm/gsm48.h`, `include/osmocom/gsm/gsm23003.h`,
`include/osmocom/gsm/protocol/gsm_04_08.h`, `include/osmocom/gsm/protocol/gsm_23_003.h`,
`src/core/utils.c` (for the BCD helpers), `include/osmocom/core/utils.h`,
`tests/gsm0408/gsm0408_test.c` and `tests/gsm0408/gsm0408_test.ok`. Each was verified
byte-for-byte against upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311`. The
runner builds with `-Wno-deprecated-declarations`, since the legacy functions are declared
deprecated.

The harness replays upstream's `test_mid_from_tmsi`, `test_mid_from_imsi`,
`test_mid_encode_decode`, `test_mid_decode_zero_length`, `test_struct_mobile_identity`,
`test_ra_cap`, `test_lai_encode_decode` and `test_osmo_routing_area_id` with the same
calls, and records them with the source `upstream`. It fails unless the output it prints
matches the two parts of `gsm0408_test.ok` those tests print, byte for byte.

## Checking

```bash
./reference/gsm48/run_gsm48_vector.sh | cmp - tests/vectors/gsm48/gsm48_vectors.ndjson
```
