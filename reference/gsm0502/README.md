# GSM 05.02 reference

`gsm0502_vector.c` calls the public `gsm0502.h` API directly and records:

- every constant in the header, including burst bit counts;
- `GSM_TDMA_FN_SUM`, `SUB`, `DIFF`, `INC`, `DEC` and `gsm0502_fncmp()` for each pair
  of 17 edge values, including out-of-range ones, and 200 pseudo-random pairs;
- `gsm0502_fn_remap()` for every frame in upstream's test output, plus sweeps
  around zero, the hyperframe boundary and the top of the `uint32_t` range;
- `gsm0502_fn2ccch_block()` over two 51-multiframes and at the boundaries;
- `gsm0502_hop_seq_gen()` with and without a mobile allocation, for all 64 hopping
  sequence numbers, with rows that expose every RNTABLE entry;
- `gsm0502_calc_paging_group()` and its helpers for every value the three-bit
  `ccch_conf`, `bs_ag_blks_res` and `bs_pa_mfrms` fields can hold.

The harness doesn't call `gsm0502_hop_seq_gen()` with an empty allocation, which
divides by zero, or with a hopping sequence number above 63, which reads past
RNTABLE. It doesn't call the group helpers with a zero divisor either.

## Fixtures

`tests/vectors/gsm0502/gsm0502_vectors.ndjson` starts with a metadata line,
followed by one section per kind of record. A `hop` record is a row of results for
frames `start`, `start + step` and so on, in `uint32_t` arithmetic.
`tests/support/gsm0502_fixture.rs` pins an FNV-1a fingerprint of the file and checks
the sections, fields, provenance and value ranges.

## Provenance

The runner checks the hashes of `src/gsm/gsm0502.c`,
`include/osmocom/gsm/gsm0502.h`, `src/gsm/gsm48.c`, `src/gsm/rsl.c`,
`src/gsm/gsm_utils.c`, `tests/gsm0502/gsm0502_test.c` and
`tests/gsm0502/gsm0502_test.ok`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The harness replays all 258 frames in
`gsm0502_test.ok` and fails if a result differs from that file.

## Checking

```bash
./reference/gsm0502/run_gsm0502_vector.sh | cmp - tests/vectors/gsm0502/gsm0502_vectors.ndjson
```
