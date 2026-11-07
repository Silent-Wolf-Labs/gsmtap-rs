# strrb reference

`strrb_vector.c` calls the public `strrb.h` API directly and records `RB_MAX_MESSAGE_SIZE`,
then a stream of events on named ringbuffers:

- `create` for `osmo_strrb_create()`, with the size requested and the size it kept;
- `add` for `osmo_strrb_add()`, with its result;
- `state` after each of those, with the start, end, `osmo_strrb_elements()`,
  `osmo_strrb_is_empty()` and `_osmo_strrb_is_bufindex_valid()` from buffer index 0;
- `nth` after each state, for `osmo_strrb_get_nth()` at indexes past the end and near the
  top of the `unsigned int` range, where adding the start wraps around.

The ringbuffers cover text around the length limit, sizes from 0 to 16 with random bytes,
and sizes that don't fit the 16-bit field.

The harness doesn't call `osmo_strrb_add()` on a ringbuffer of size 0, which crashes.

## Fixtures

`tests/vectors/strrb/strrb_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record. `tests/support/strrb_fixture.rs` pins an FNV-1a fingerprint of
the file and checks the sections, fields, provenance and value ranges.

## Provenance

The runner checks the hashes of `src/core/strrb.c`, `include/osmocom/core/strrb.h`,
`tests/strrb/strrb_test.c` and `tests/strrb/strrb_test.ok`, verified byte-for-byte against
upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311`.

Upstream's test checks its results with assertions and prints one line. The harness makes
the same calls and checks, records those calls with the source `upstream`, and fails if a
check or that line differs.

## Checking

```bash
./reference/strrb/run_strrb_vector.sh | cmp - tests/vectors/strrb/strrb_vectors.ndjson
```
