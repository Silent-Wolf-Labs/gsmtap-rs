# GEA reference

`gea_vector.c` calls the public `gprs_cipher.h` API directly and records:

- `gprs_cipher_supported()`, `gprs_cipher_key_length()` and the `gprs_cipher_names` entry
  for every algorithm and values out of range;
- `gprs_cipher_gen_input_ui()` for every SAPI and a sweep of IOV-UI, LFN and OC values, and
  `gprs_cipher_gen_input_i()` for the same values;
- `gprs_cipher_run()` for GEA3 and GEA4, covering:
  - upstream's vectors;
  - lengths from 0 to the 1,523-byte maximum;
  - fixed keys and inputs in each direction, directions past 1 bit, each key bit alone
    and random inputs;
  - the results for GEA0, GEA1, GEA2 and values out of range, and for blocks that are
    too long.

Each run record holds the output written, then the four guard bytes after it. The harness
fails if a run writes past its length, or writes anything and returns an error. The key is
passed in a 16-byte buffer, as upstream's test does; GEA3 reads its first 8 bytes.

Not recorded: `gea3()` and `gea4()` called directly, which `gprs_cipher_run()` calls and
which aren't in libosmogsm's exported symbols; `gprs_cipher_register()` and
`gprs_cipher_load()`, which add plugins; and `gprs_cipher_gen_input_ui()` for SAPIs from
16 on, where its arithmetic overflows an `int`.

## Fixtures

`tests/vectors/gea/gea_vectors.ndjson` starts with a metadata line, followed by one section
per kind of record. `tests/support/gea_fixture.rs` pins an FNV-1a fingerprint of the file
and checks:
- the sections and fields;
- which algorithms C supports and names;
- that every `gprs_cipher_gen_input_i()` result is the sum of its inputs;
- each run's result code, key length and guard bytes.

## Provenance

The runner checks the hashes of:
- `src/gsm/gea.c`, `src/gsm/gprs_gea.c`, `src/gsm/gprs_cipher_core.c`, `src/gsm/kasumi.c`
  and `src/gsm/auth_core.c`, for `osmo_c4()`;
- `include/osmocom/crypt/gprs_cipher.h`, `include/osmocom/gsm/gea.h` and
  `include/osmocom/gsm/kasumi.h`;
- `tests/gea/gea_test.c` and `tests/gea/gea_test.ok`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

The harness reads every vector from `gea_test.c`, rebuilds `gea_test.ok` from its own
calls, and fails if a line differs. Upstream's vectors are those of 3GPP TS 55.217, 55.218
and 55.226, and four recorded LLC frames.

## Checking

```bash
./reference/gea/run_gea_vector.sh | cmp - tests/vectors/gea/gea_vectors.ndjson
```
