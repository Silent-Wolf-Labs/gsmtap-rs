# 3GPP TS 23.236 NRI reference

`gsm23236_vector.c` calls libosmogsm's Network Resource Indicator (NRI) functions from
`gsm23236.h` directly. They support MSC and SGSN pooling: the NRI in a TMSI says which node
of a pool gave it out. It writes one section per kind of record:

- `tmsi_get` and `tmsi_set` for `osmo_tmsi_nri_v_get()` and `osmo_tmsi_nri_v_set()` on
  TMSIs with every bit length from 0 to 16 and 255, and NRIs from `INT16_MIN` to
  `INT16_MAX`;
- `validate` and `range_validate` for `osmo_nri_v_validate()` and
  `osmo_nri_range_validate()` at each power of two and one under it, the ends of
  `int16_t`, and the same bit lengths;
- `add`, `del`, `matches` and `overlaps` for `osmo_nri_ranges_add()`,
  `osmo_nri_ranges_del()`, `osmo_nri_v_matches_ranges()` and
  `osmo_nri_range_overlaps_ranges()` on every list of the values 0 to 4, with every range
  whose ends run from -1 to 5, and on ranges at the ends of `int16_t`;
- `limit` and `tmsi_limit` for `osmo_nri_v_limit_by_ranges()` and
  `osmo_tmsi_nri_v_limit_by_ranges()` on the same lists with bit lengths of 0 to 3, 15,
  16, 17 and 30, and on ranges partly or wholly past 8 to 15 bits;
- `to_str` and `to_str_c` for `osmo_nri_ranges_to_str_buf()` with every buffer length up
  to two past the text, and `osmo_nri_ranges_to_str_c()` on texts of 15 to 17 characters
  around its first buffer of 16 bytes;
- `vty_add` and `vty_del` for `osmo_nri_ranges_vty_add()` and `osmo_nri_ranges_vty_del()`
  with one or two arguments, a missing first one, decimal and hexadecimal text, whitespace,
  signs, trailing characters and values past `INT16_MAX`, and bit lengths of 0, 1, 3, 15,
  16 and 255.

Lists are written as `[first..last]` for each entry in list order, and C strings as hex.
Out-parameters hold `0x5555` beforehand and text buffers `0x55`, so a record shows what a
call wrote.

`osmo_nri_v_limit_by_ranges()` divides by the number of the list's values that fit in the
bit length. Each limit call runs in a child process that catches fatal signals, and the
record holds the signal or the call's results.

## Recorded C behavior

- **NRI in a TMSI:** the NRI is the bits from bit 23 down. Getting it takes bit lengths of
  1 to 15 only, and writes -1 for the others. Setting it keeps only the low bits of an NRI
  that is too large, and rejects a negative one, leaving the TMSI alone.
- **Validation:** a negative value gives -1, and one past the bit length 1. A bit length of
  0 takes no value, and from 15 on only the sign is checked. A range gives the first
  value's result, then the last value's doubled, then -3 if the first is past the last.
- **Lists:** adding joins every range the new one overlaps or touches, and deleting cuts or
  splits them, so lists stay sorted with ranges apart. A negative value or a first value
  past the last returns -1 with the list unchanged.
- **Overlaps:** a range overlaps an entry when an end of either lies within the other. A
  range whose first value is past its last holds nothing, but still overlaps an entry that
  holds one of its ends.
- **Limit:** a value from 0 picks the list's value at it modulo the number of values that
  fit in the bit length, in order. A bit length of 0 leaves only the value 0. Past 15 bits
  C's `int16_t` maximum wraps to -1 and no value fits. Where no value fits, as with an
  empty list, C divides by zero and the process ends by `SIGFPE`. The TMSI form fails first
  for a bit length other than 1 to 15.
- **Text:** `empty`, or each entry as `first..last`. The documentation gives
  `23..42,123..142`, but no commas are written: that list gives `23..42123..142`. The
  buffer form truncates as `snprintf` does and returns the full length.
- **VTY:** each argument is parsed with `osmo_str_to_int()`, in base 16 when it starts with
  `0x` and otherwise in base 10, from 0 to `INT16_MAX`. As `strtoll` does, base 10 skips
  leading whitespace and takes a sign, so ` 1`, `+1` and `-0` parse. `0X1f`, a lone `0x`,
  and trailing characters don't. A missing first argument takes the second as the only
  one. The parsed range is written only when both arguments parse. Adding a range past a
  bit length of at most 15 adds it but returns 1 with a warning.

## Fixtures

`tests/vectors/gsm23236/gsm23236_vectors.ndjson` starts with a metadata line, followed by
one section per kind of record. `tests/support/gsm23236_fixture.rs` pins an FNV-1a
fingerprint of the file and checks the sections, fields, provenance and result codes. It
also checks that lists are sorted with valid ranges apart, that failed calls left their
inputs alone, that a limit result is in its list and only sweeps end by `SIGFPE`, that
text buffers end with a NUL and an untouched byte, and that VTY messages match their
results.

## Provenance

The runner checks the hashes of `src/gsm/gsm23236.c`, `include/osmocom/gsm/gsm23236.h`,
`src/core/utils.c` (for `osmo_str_to_int()`), `include/osmocom/core/utils.h` (for the
string buffer macros), `include/osmocom/core/linuxlist.h`, `tests/gsm23236/gsm23236_test.c`
and `tests/gsm23236/gsm23236_test.ok`. Each was verified byte-for-byte against upstream
revision `950430e829a3dc1d162aa241bc0505745c5a7311`; `gsm23236.c` is unchanged on master
as of 2026-11-18.

The harness replays upstream's `gsm23236_test` with the same calls, and records them with
the source `upstream`. It fails unless the output it prints matches `gsm23236_test.ok`
byte for byte.

## Checking

```bash
./reference/gsm23236/run_gsm23236_vector.sh | cmp - tests/vectors/gsm23236/gsm23236_vectors.ndjson
```
