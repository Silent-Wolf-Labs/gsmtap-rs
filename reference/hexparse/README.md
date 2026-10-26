# Hexparse reference

`hexparse_vector.c` calls `osmo_hexparse()` directly. Each observation records
the C string input including its NUL terminator, the initialized destination,
its capacity, the return code and the complete final destination.

## Fixtures

`tests/vectors/hexparse/hexparse_buffer_vectors.json` is a JSON array of 21
observations.

## Provenance

The runner checks the hashes of `src/core/utils.c` and
`include/osmocom/core/utils.h`. Those files and `tests/utils/utils_test.c` were
compared byte-for-byte with upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The runner then rebuilds `src/core`
and links against it.

## Checking

```bash
./reference/hexparse/run_hexparse_vector.sh | cmp - tests/vectors/hexparse/hexparse_buffer_vectors.json
```

`tests/hexparse_reference_vectors.rs` checks the fixture's structure and
provenance.
