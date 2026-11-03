# gsm_utils reference

## Numeric helpers

`gsm_utils_numeric_vector.c` calls the public `gsm_utils.h` API directly and records:

- the frame, ARFCN flag, band and TLLI constants in the header;
- `gsm_band_name()` and `gsm_band_parse()`, with band names, prefixes, suffixes,
  leading zeros and values too large for `long`;
- `ms_class_gmsk_dbm()`, `ms_pwr_ctl_lvl()` and `ms_pwr_dbm()` for each band and for
  codes that name no band, covering every `uint8_t` power level;
- `rxlev2dbm()` for every `uint8_t`, and `dbm2rxlev()` around its range and at
  extremes;
- `gsm_arfcn2band_rc()` and `gsm_arfcn2freq10()` for every ARFCN under each flag
  combination, including whether the call wrote its output;
- `gsm_freq102arfcn()` for every frequency in the band table and some outside it;
- `gsm_fn2gsmtime()`, `gsm_gsmtime2fn()`, `gsm_fn2rfn()` and
  `gsm_fn_as_gsmtime_str()` at frame numbers including past the hyperframe, where
  `t1` truncates;
- `gsm_gsmtime2fn()` and `osmo_dump_gsmtime()` on `gsm_time` values whose fields are
  out of range or disagree;
- `gsm_rfn2fn()` around the wrap threshold and the hyperframe boundary;
- `gprs_tlli_type()`, `gprs_tlli2tmsi()` and `gprs_tmsi2tlli()`, including type
  values with no conversion;
- `ms_cm2_a5n_support()`, `ms_cm3_a5n_support()` and the RACH helpers.

The harness doesn't call `gsm_rfn2fn()` with an RFN of `GSM_RFN_MODULUS` or more,
which aborts, or `dbm2rxlev()` within 110 of `INT_MAX`, where the addition overflows.

## 7-bit functions

`gsm_utils_7bit_vector.c` calls the public API directly and records:

- `gsm_get_octet_len()` for every `uint8_t`;
- `gsm_septet_encode()` for every byte from 0x01 to 0xfe and some longer strings;
- `gsm_septet_pack()` for each padding from 0 to 7, at each length to 24 and at 300;
- `gsm_7bit_encode_n()` and `gsm_7bit_encode_n_ussd()` under a range of output sizes,
  including limits that split an escape and text whose last septet is `<CR>`;
- `gsm_7bit_decode_n()` for each septet alone and after an escape, for escapes at the
  end and in a row, and for random input under output limits;
- `gsm_7bit_decode_n_ussd()` on encoder output at septet counts either side of the one
  it returned, and on random input with `<CR>` in the top bits of its last octet;
- `gsm_7bit_decode_n_hdr()` with each header length to 12 at septet counts from where it
  ends, and with headers longer than the message.

Longer headers make C's septet count wrap, so it reads past the octets the count
implies. Those records carry a 512-byte input, which covers its furthest read, and have
the source `wrap`. Other inputs hold every octet C reads.

The harness doesn't call `gsm_septet_encode()` or the encoders with byte 0xff, which reads
past C's table, `gsm_septet_pack()` with padding above 7, which shifts by a negative
amount, or the decoders with `n` of 0, which aborts. It also checks that no call writes
past the output it reports or the output size it was given.

## Not recorded

`gsm_utils.h` has more than this. The deprecated 7-bit wrappers, which take no output
size, and `gsm_septets2octets()`, which wraps `gsm_septet_pack()`, aren't recorded.
`gsm_arfcn2band()`, which aborts on an invalid ARFCN, and `osmo_get_rand_id()`, which
reads OS randomness, aren't recorded. Nor are `osmo_dump_gsmtime_buf()` and
`osmo_dump_gsmtime_c()`, which format like `osmo_dump_gsmtime()` into a caller's
buffer or a talloc context.

## Fixtures

`tests/vectors/gsm_utils/gsm_utils_numeric_vectors.ndjson` and
`gsm_utils_7bit_vectors.ndjson` each start with a metadata line, followed by one section
per kind of record. `tests/support/gsm_utils_fixture.rs` and
`gsm_utils_7bit_fixture.rs` pin an FNV-1a fingerprint of their file and check the
sections, fields, provenance and value ranges.

## Provenance

The numeric runner checks the hashes of `src/gsm/gsm_utils.c`,
`include/osmocom/gsm/gsm_utils.h`, `include/osmocom/gsm/gsm0502.h`,
`include/osmocom/core/utils.h`, `tests/gsm0408/gsm0408_test.c` and
`tests/gsm0408/gsm0408_test.ok`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

`gsm0408_test.c` covers much more than these helpers, so the whole test isn't replayed.
The harness replays the 16 RACH lines from `gsm0408_test.ok` and the test's own
checks on `ms_class_gmsk_dbm()`, `gsm_gsmtime2fn()` and `gsm_rfn2fn()`, and fails if a
result differs. Those checks print nothing, so their expected values are written in
the harness.

The 7-bit runner checks the hashes of `src/gsm/gsm_utils.c`,
`include/osmocom/gsm/gsm_utils.h`, `include/osmocom/core/utils.h`,
`tests/sms/sms_test.c` and `tests/sms/sms_test.ok`, at the same revision. `sms_test.c`
also covers `gsm340_gen_oa()`, so its output isn't replayed whole. The harness copies
the test's text and encoded messages, repeats its checks on the 7-bit functions, and
compares its results with the 25 matching lines of `sms_test.ok`.

## Checking

```bash
./reference/gsm_utils/run_gsm_utils_numeric_vector.sh | cmp - tests/vectors/gsm_utils/gsm_utils_numeric_vectors.ndjson
./reference/gsm_utils/run_gsm_utils_7bit_vector.sh | cmp - tests/vectors/gsm_utils/gsm_utils_7bit_vectors.ndjson
```
