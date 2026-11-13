# TUAK reference

`tuak_vector.c` calls libosmogsm's TUAK functions (3GPP TS 35.231) directly and records:

- `tuak_opc_gen()` for upstream's six test sets, fixed patterns and random keys, with 16
  and 32-byte keys, key lengths C rejects and one to three Keccak iterations;
- `tuak_f1()` and `tuak_f1star()` for upstream's test sets and every MAC length with both
  key lengths, and for MAC and key lengths C rejects;
- `tuak_f2345()` for upstream's test sets and every combination of RES, CK, IK and key
  lengths, and for each length C rejects;
- `tuak_f5star()` for upstream's test sets, both key lengths and random inputs;
- `osmo_auth_gen_vec2()` and `osmo_auth_gen_vec_auts2()` for TUAK, covering:
  - every RES length from 0 to 32, of which C accepts 4, 8 and 16;
  - key and OPc lengths, and OP instead of OPc;
  - SQN, including values past 48 bits and at the top of 64, and IND lengths and slots;
  - valid and corrupted AUTS, and AUTS that passes before the vector fails;
  - random subscribers.

The TUAK functions are in no exported symbol list. The runner links the internal
`libgsmint.a` archive that `libosmogsm.so` is built from, as upstream's TUAK test does.
Nothing refers to `auth_tuak.o`, whose constructor registers TUAK with the auth core, so
the runner links that object by name as well.

Each function record holds the bytes written, then the four guard bytes after them, and
the harness fails if a function writes past them. `tuak_opc_gen()` takes its Keccak
iteration count from a global, which the harness sets for each record and resets to 1;
the vector records all run with 1. Vector records hold the whole `osmo_auth_vector`,
prefilled with `0x55` apart from the RES length the caller sets, and the subscriber's SQN
and SQN_MS after the call.

The harness builds valid AUTS inputs with `tuak_f5star()` and `tuak_f1star()`, as 3GPP
TS 33.102 section 6.3.3 describes; every recorded result comes from the call under test.
C checks AUTS with all 32 bytes of `aud->u.umts.k`, whatever `k_len` says, so for a 16-byte
K it rejects an AUTS made from that K and accepts one made from the whole buffer as a
32-byte key. The fixture records both.

## Fixtures

`tests/vectors/tuak/tuak_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record, each function's starting with its upstream records.
`tests/support/tuak_fixture.rs` pins an FNV-1a fingerprint of the file and checks:
- the sections and fields;
- that each function succeeds exactly for the lengths TUAK defines;
- each output's length and guard bytes;
- that a failed vector call left the vector untouched, and a successful one wrote RES only
  as long as asked.

## Provenance

The runner checks the hashes of:
- `src/gsm/auth_tuak.c`, `src/gsm/auth_core.c`;
- `src/gsm/tuak/tuak.c`, `tuak.h`, `KeccakP-1600-3gpp.c` and `KeccakP-1600-3gpp.h`;
- `include/osmocom/crypt/auth.h`;
- `tests/auth/tuak_test.c` and `tests/auth/tuak_test.ok`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. `include/osmocom/core/bit64gen.h` is generated
by the build, so it isn't pinned.

The harness reads all six test sets from `tuak_test.c`, rebuilds `tuak_test.ok` from its
own calls and fails if a line differs. It also checks each set's f5*, which `tuak_test.c`
gives but doesn't print. The test sets are those of 3GPP TS 35.233 sections 6.3 to 6.8.

## Checking

```bash
./reference/tuak/run_tuak_vector.sh | cmp - tests/vectors/tuak/tuak_vectors.ndjson
```
