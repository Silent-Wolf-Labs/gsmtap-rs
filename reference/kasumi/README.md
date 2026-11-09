# KASUMI reference

`kasumi_vector.c` calls libosmogsm's KASUMI functions directly and records:

- `_kasumi_key_expand()` for upstream's keys, fixed patterns, each key bit alone and random
  keys;
- `_kasumi()` for upstream's test sets, including each step of test set 4's 50 chained
  rounds, and for fixed patterns, each input and key bit alone and random blocks;
- `_kasumi_kgcore()` for upstream's five sets and a sweep of lengths from 0 to 65,535 bits,
  CA, COUNT, bearer and direction values, keys and random inputs;
- `_a5_3()` and `_a5_4()` with a raw COUNT (`fn_correct` false), for upstream's vectors,
  fixed patterns, each COUNT bit alone and random inputs.

These functions are in no exported symbol list. The runner links the internal
`libgsmint.a` archive that `libosmogsm.so` is built from, as upstream's KASUMI and A5
tests do.

Each KGCORE record holds the bytes written for its length in bits, rounded up, then the
four guard bytes after them. The harness fails if KGCORE writes past those bytes. Bearers
from 32 on and directions from 2 on are recorded too: C shifts them into the bits next to
their fields. For each A5 record the harness also checks that asking for one direction
alone gives the same bits.

`_a5_4()` reads one byte past the 29 its uplink keystream fills, but only into bits it
drops, so the recorded uplink doesn't depend on it. `osmo_a5()` for A5/3 and A5/4, which
takes a frame number, is recorded by the [A5 harness](../a5/README.md).

## Fixtures

`tests/vectors/kasumi/kasumi_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record, each starting with its upstream records.
`tests/support/kasumi_fixture.rs` pins an FNV-1a fingerprint of the file and checks:
- the sections and fields;
- that KI1, KI2 and KI3 are rotations of KL2, as TS 35.202 defines them;
- each KGCORE record's length and guard bytes;
- that A5/3 has an 8-byte key and A5/4 a 16-byte key.

## Provenance

The runner checks the hashes of:
- `src/gsm/kasumi.c`, `src/gsm/a5.c` and `src/gsm/auth_core.c`, for `osmo_c4()`;
- `include/osmocom/gsm/kasumi.h`, `include/osmocom/gsm/a5.h` and
  `include/osmocom/crypt/auth.h`;
- `tests/kasumi/kasumi_test.c`, `tests/kasumi/kasumi_test.ok`, `tests/a5/a5_test.c` and
  `tests/a5/a5_test.ok`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

The harness reads every vector from `kasumi_test.c` and `a5_test.c`. It rebuilds
`kasumi_test.ok` and the 24 A5/3 and A5/4 lines of `a5_test.ok` from its own calls, and
fails if a line differs. Upstream's KASUMI and KGCORE vectors are those of 3GPP TS 35.203
and TR 55.919, and its A5/3 and A5/4 vectors those of TS 55.217, 55.218 and 55.226.

## Checking

```bash
./reference/kasumi/run_kasumi_vector.sh | cmp - tests/vectors/kasumi/kasumi_vectors.ndjson
```
