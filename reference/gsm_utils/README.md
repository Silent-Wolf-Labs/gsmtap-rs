# gsm_utils reference

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

`gsm_utils.h` has more than this. Its 7-bit functions aren't recorded yet, and
`gsm_arfcn2band()`, which aborts on an invalid ARFCN, and `osmo_get_rand_id()`, which
reads OS randomness, aren't recorded. Nor are `osmo_dump_gsmtime_buf()` and
`osmo_dump_gsmtime_c()`, which format like `osmo_dump_gsmtime()` into a caller's
buffer or a talloc context.

## Fixtures

`tests/vectors/gsm_utils/gsm_utils_numeric_vectors.ndjson` starts with a metadata
line, followed by one section per kind of record. `tests/support/gsm_utils_fixture.rs`
pins an FNV-1a fingerprint of the file and checks the sections, fields, provenance
and value ranges.

## Provenance

The runner checks the hashes of `src/gsm/gsm_utils.c`,
`include/osmocom/gsm/gsm_utils.h`, `include/osmocom/gsm/gsm0502.h`,
`include/osmocom/core/utils.h`, `tests/gsm0408/gsm0408_test.c` and
`tests/gsm0408/gsm0408_test.ok`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

`gsm0408_test.c` covers much more than these helpers, so the whole test isn't replayed.
The harness replays the 16 RACH lines from `gsm0408_test.ok` and the test's own
checks on `ms_class_gmsk_dbm()`, `gsm_gsmtime2fn()` and `gsm_rfn2fn()`, and fails if a
result differs. Those checks print nothing, so their expected values are written in
the harness.

## Checking

```bash
./reference/gsm_utils/run_gsm_utils_numeric_vector.sh | cmp - tests/vectors/gsm_utils/gsm_utils_numeric_vectors.ndjson
```
