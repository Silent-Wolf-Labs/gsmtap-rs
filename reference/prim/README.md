# Primitive reference

`prim_vector.c` calls the `osmo_prim_*` helpers directly: header
initialization and reinitialization, operation names, packed IDs, message
association and event-map lookup, plus one callback through the C callback
typedef.

## Fixtures

`tests/vectors/prim/prim_vectors.json` holds 6,865 records:

| Kind | Records | Coverage |
| --- | ---: | --- |
| Initialization | 2,112 | Six SAPs including all three namespace bases, eight primitive IDs, eleven operation values, with and without an attached message, in two phases |
| Event map | 4,752 | Empty maps, exact and single-field mismatches, duplicate first matches, sentinel before a match, zero events and `UINT32_MAX - 1` |
| Callback | 1 | Header, payload and context mutation and a negative return |

The second initialization phase complements every numeric field and toggles
attachment. Together the phases cover zero and maximum values, packed-ID
overflow, operations whose low bytes collide, all four known names and the
fallback for unknown names. Every initialization and event record snapshots
all 16 backing bytes, data, tail and length, the four layer offsets and the five
control words. Attachment is checked by pointer identity.

`tests/support/prim_fixture.rs` checks the exact schema, ordered input coverage,
map entries, ABI and source hashes, and pins an FNV-1a fingerprint of the whole
file.

## Recorded C behavior

Event-map lookup compares the full raw operation value, so entries whose packed
low bytes coincide still do not match.

## Excluded cases

Unsafe pointers, unterminated maps and the raw embedded-header layout are never
cases.

## Provenance

The runner checks the hashes of `prim.c`, `prim.h`, `utils.c`, `utils.h`,
`msgb.c` and `msgb.h`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. It requires 32-bit `unsigned int`
and operation enums and 64-bit `unsigned long` and pointers.

## Checking

```bash
./reference/prim/run_prim_vector.sh | cmp - tests/vectors/prim/prim_vectors.json
```
