# A5 reference

`a5_vector.c` calls the public `a5.h` API directly and records:

- `osmo_a5_fn_count()` at frame numbers including past the hyperframe, and at random
  frame numbers;
- both directions of `osmo_a5()` for A5/0, A5/1 and A5/2, with fixed keys at each of a
  set of frame numbers, each key bit and each frame count bit alone, and random keys
  and frame numbers;
- the result of `osmo_a5()` for cipher numbers with no algorithm.

For every stream, the harness also checks that asking for one direction alone gives
the same bits, that an error leaves the output alone, and that A5/0 needs no key.

The harness doesn't use frame numbers from 1326 x 2^20 on, where a shift in
`osmo_a5_fn_count()` overflows an `int`, or call A5/1 or A5/2 with no key, which crashes.

A5/3 and A5/4 aren't recorded, because they need KASUMI and `osmo_c4()`, which aren't
ported. Nor are the deprecated `osmo_a5_1()` and `osmo_a5_2()`, which call `osmo_a5()`.

## Fixtures

`tests/vectors/a5/a5_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record. `tests/support/a5_fixture.rs` pins an FNV-1a fingerprint of
the file and checks the sections, fields, provenance and value ranges.

## Provenance

The runner checks the hashes of `src/gsm/a5.c`, `include/osmocom/gsm/a5.h`,
`tests/a5/a5_test.c` and `tests/a5/a5_test.ok`, verified byte-for-byte against upstream
revision `950430e829a3dc1d162aa241bc0505745c5a7311`.

The harness replays the 6 A5/0, A5/1 and A5/2 lines of `a5_test.ok` and fails if a
result differs. The rest of that file covers A5/3 and A5/4.

## Checking

```bash
./reference/a5/run_a5_vector.sh | cmp - tests/vectors/a5/a5_vectors.ndjson
```
