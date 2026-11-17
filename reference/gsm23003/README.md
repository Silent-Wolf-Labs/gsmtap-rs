# 3GPP TS 23.003 identity reference

`gsm23003_vector.c` calls libosmogsm's `gsm23003.h` API directly and records the
`gsm_23_003.h` digit and length constants, then one section per kind of record:

- `valid` for `osmo_imsi_str_valid()`, `osmo_msisdn_str_valid()` and
  `osmo_imei_str_valid()` without and with the check digit. Inputs are digit strings of
  every length up to 17, each check's shortest and longest lengths with a non-digit first,
  in the middle and last, NULL, and every check digit after ten 14-digit IMEIs, of which
  the harness checks exactly one passes;
- `from_str` for `osmo_mnc_from_str()` and `osmo_mcc_from_str()` on the same text. Both
  results hold junk beforehand, so the record shows whether a failed call writes them;
- `name` for the MCC, MNC, PLMN, LAI, RAI, CGI, CGI-PS, SAI, RNC ID and GUMMEI names. Every
  variant of a name, static, second static, `_buf` and talloc'd, must give the same text,
  which the record holds once;
- `name_buf` for `osmo_plmn_name_buf()`, `osmo_cgi_ps_name_buf()` and
  `osmo_gummei_name_buf()` with every buffer length up to one past the name;
- `cmp` for `osmo_mnc_cmp()` over every pair of MNC values and flags, and for each other
  `osmo_*_cmp()` over every pair of values that differ in each field in turn;
- `gen` for `osmo_gen_home_network_domain()`, `osmo_gen_mme_domain()` and
  `osmo_gen_mme_group_domain()`;
- `parse` for `osmo_parse_home_network_domain()` and `osmo_parse_mme_domain()` on valid
  domains and on short, long, signed, spaced, upper-case and cut-off fields, other
  suffixes and none.

MCC and MNC values cover 0, 99, 100, 999, 1000 and 65535, with both MNC flags. The other
fields cover their edges. Names and domains are written into buffers of `0x55`, and the
harness fails if a call writes past its text and NUL, or writes anything when it fails.

## Recorded C behavior

- **MNC flag:** `osmo_mnc_cmp()` ignores the three-digit flag from MNC 100 on, and so do
  the comparisons built on it. A name prints three digits for such an MNC whatever the
  flag says.
- **Values past three digits:** names and comparisons take any 16-bit MCC or MNC. The
  domain generators return `-EINVAL` for one past 999.
- **Domain parsers:** these use `sscanf()`, so they read each field as up to two, three or
  four characters, with leading spaces and a sign, and `osmo_parse_mme_domain()` takes a
  `0x` prefix. They don't check the text after the MCC, so other suffixes and none are
  accepted, and a field followed by more digits is cut. A negative field wraps to a 16 or
  8-bit value. The parsed MNC's three-digit flag is always false. Text that ends before the
  first field returns -1, `sscanf()`'s `EOF`. On `-EINVAL`, fields read before the
  failure keep their values.
- **IMEI check digit:** the Luhn digit is checked only with `with_15th_digit`. Without it,
  15 digits fail.

## Fixtures

`tests/vectors/gsm23003/gsm23003_vectors.ndjson` starts with a metadata line, followed by
the constants and one section per kind of record. `tests/support/gsm23003_fixture.rs` pins
an FNV-1a fingerprint of the file and checks the sections, fields, provenance, result
codes, that failed calls left their results untouched and that short buffers end at a NUL.

## Provenance

The runner checks the hashes of `src/gsm/gsm23003.c`, `include/osmocom/gsm/gsm23003.h`,
`include/osmocom/gsm/protocol/gsm_23_003.h`, `src/core/utils.c` (for `osmo_luhn()` and
`osmo_str_to_int()`), `include/osmocom/core/utils.h`, `tests/gsm23003/gsm23003_test.c` and
`tests/gsm23003/gsm23003_test.ok`. Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

`libosmogsm.so` doesn't export `osmo_sai_name()` or its variants, although `gsm23003.h`
declares them. The runner links the internal `libgsmint.a` archive that the shared library
is built from, as upstream's tests do.

The harness replays upstream's test with the same calls, records them with the source
`upstream`, and fails unless the output it prints matches `gsm23003_test.ok` byte for byte.

## Checking

```bash
./reference/gsm23003/run_gsm23003_vector.sh | cmp - tests/vectors/gsm23003/gsm23003_vectors.ndjson
```
