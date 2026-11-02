# PRBS reference

`prbs_vector.c` calls `osmo_prbs_state_init()`, `osmo_prbs_get_ubit()` and
`osmo_prbs_get_ubits()` directly. Each record holds one call: the descriptor, the
return value, the generator state afterwards and, for `osmo_prbs_get_ubits()`,
the complete destination. Every destination byte starts as `0xa5`, and four guard
bytes follow the requested length.

Cases cover two full periods of each of the four standard sequences, single-bit
steps, mixed and zero-length calls, and caller-defined descriptors: a zero
coefficient, a fixed point, a 64-bit register and a length the generator ignores.

## Fixtures

`tests/vectors/prbs/prbs_vectors.ndjson` starts with a metadata line, followed by
one record per call. `tests/support/prbs_fixture.rs` pins an FNV-1a fingerprint of
the file and checks the case names and order, fields, step order, provenance and
the buffer contract: output is unpacked bits, the guard bytes stay untouched and
`osmo_prbs_get_ubits()` returns its length.

## Provenance

The runner checks the hashes of `src/core/prbs.c`,
`include/osmocom/core/prbs.h`, `tests/prbs/prbs_test.c` and
`tests/prbs/prbs_test.ok`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The two-period records contain the
same bit sequences as `prbs_test.ok`.

## Checking

```bash
./reference/prbs/run_prbs_vector.sh | cmp - tests/vectors/prbs/prbs_vectors.ndjson
```
