# use_count reference

`use_count_vector.c` calls the public `use_count.h` API directly and records a stream of
events on named objects:

- `new`, `static` and `free` for creating an object, `osmo_use_count_make_static_entries()`
  and `osmo_use_count_free()`;
- `call` and `result` around each `osmo_use_count_get_put()`, with its return value;
- `callback` for each callback a call runs, so nested calls appear in order;
- `to_str` for `osmo_use_count_to_str_buf()` at a given size.

Events that change an object also record its total, its text and every entry in list
order, including idle ones. The harness checks that `osmo_use_count_name_buf()` and
`osmo_use_count_to_str_c()` give the same text, and that no call writes past its size.

The scripted objects cover an object with no entries, order and reuse of entries, static
entries, NULL and empty tokens, and the limits of a count and of the total. Others make
random calls under each callback behaviour: none, returning 0, returning -5, clamping to 1,
freeing an entry at zero, and what upstream's test does.

The harness doesn't:

- call `osmo_use_count_free()` on a static entry, which C forbids;
- let a count reach `INT32_MIN` while a negative total comes before it, or pass
  `INT32_MIN` as a change to a negative count, because C's negation then overflows;
- call `osmo_use_count_to_str_buf()` or `osmo_use_count_find()` with no object, which crash;
- set a token to NULL from a callback, which exists for tokens that stop being valid.

## Fixtures

`tests/vectors/use_count/use_count_vectors.ndjson` starts with a metadata line, followed
by one section per kind of record. `tests/support/use_count_fixture.rs` pins an FNV-1a
fingerprint of the file and checks the sections, fields, provenance and value ranges.

## Provenance

The runner checks the hashes of `src/core/use_count.c`, `include/osmocom/core/use_count.h`,
`tests/use_count/use_count_test.c` and `tests/use_count/use_count_test.err`, verified
byte-for-byte against upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311`.

Upstream's test checks its results through logs, in `use_count_test.err`. The harness runs
the test's sequence with its FSM reduced to the two states the test uses, and compares
its use count, error and object list lines with the matching lines of that file. It
doesn't compare the FSM's own log lines.

## Checking

```bash
./reference/use_count/run_use_count_vector.sh | cmp - tests/vectors/use_count/use_count_vectors.ndjson
```
