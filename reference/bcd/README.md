# BCD reference

`bcd_conversion_vector.c` calls `osmo_bcd2char()`, `osmo_char2bcd()`,
`osmo_bcd2str()` and `osmo_str2bcd()` directly. Each observation records the
source bytes, offsets, flags, the initial and final destination bytes and the C
return value.

## Fixtures

`tests/vectors/bcd/bcd_conversion_vectors.json` is a JSON array of 76
observations. The cases use only in-bounds C arrays.

## Provenance

The runner checks the hashes of `src/core/utils.c`, `include/osmocom/core/utils.h`
and `tests/utils/utils_test.c`, verified against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. It then rebuilds `src/core` and
links against it.

## Checking

```bash
./reference/bcd/run_bcd_conversion_vector.sh | cmp - tests/vectors/bcd/bcd_conversion_vectors.json
```

`tests/bcd_reference_vectors.rs` checks the fixture's structure and provenance.
