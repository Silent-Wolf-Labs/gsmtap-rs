# Authentication reference

`auth_vector.c` calls libosmogsm directly and records:

- `osmo_auth_alg_name()` for every algorithm value and one past the end,
  `osmo_auth_alg_parse()` for exact, differently cased and unknown names, and
  `osmo_sub_auth_type_names`;
- `osmo_auth_c2()` with both derivation functions for every RES length from 0 to 16,
  `osmo_auth_c3()`, `osmo_c4()`, and `osmo_auth_3g_from_2g()` for each combination of the
  GSM and UMTS types;
- the Milenage functions, with the inputs of 3GPP TS 35.207 test set 1, fixed patterns and
  random values:
  - `milenage_opc_gen()`;
  - `milenage_f1()`, `milenage_f2345()` and `gsm_milenage()`;
  - `milenage_auts()` for valid, corrupted and mismatched AUTS;
  - `milenage_check()` for a newer, equal, older and much newer SQN and a bad MAC or AMF;
- `osmo_auth_gen_vec2()` and `osmo_auth_gen_vec_auts2()` for Milenage and XOR-3G, covering:
  - SQN, including values past 48 bits and at the top of 64;
  - IND lengths and slots, including one past the limit;
  - RES lengths, key and OPc lengths, OP instead of OPc;
  - GSM, UMTS and other subscriber types;
  - algorithms with no implementation or no resync.

Each vector record holds the whole `osmo_auth_vector`, prefilled with `0x55` apart from
the RES length the caller sets. It also holds the subscriber's SQN and SQN_MS after the
call, or its Ki for a GSM subscriber.

The harness builds valid AUTS inputs: for Milenage with `milenage_f1()` and
`milenage_f2345()`, and for XOR-3G from Ki XOR RAND, as 3GPP TS 34.108 section 8.1.2.2
describes. Only the inputs come from these; every recorded result comes from the call
under test. The Milenage functions and `osmo_auth_3g_from_2g()` are exported by libosmogsm
but declared in no installed header, so the harness declares them.

Not recorded: the deprecated `osmo_auth_gen_vec()` and `osmo_auth_gen_vec_auts()`, apart
from the upstream test replay; TUAK, which the [TUAK harness](../tuak/README.md) covers;
and cases where C reads a union member the subscriber type doesn't fill.

## Fixtures

`tests/vectors/auth/auth_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record. Vector and resync records come last, each kind in one section.
`tests/support/auth_fixture.rs` pins an FNV-1a fingerprint of the file and checks:
- each kind's fields, in order, and their types and hex lengths;
- the result codes and enum values C can return;
- that a failed call left the vector untouched;
- that the algorithm names come in C's order.

## Provenance

The runner checks the hashes of:
- `src/gsm/auth_core.c`, `src/gsm/auth_milenage.c`, `src/gsm/auth_xor.c`;
- `src/gsm/milenage/milenage.c`, `milenage.h`, `aes-encblock.c`, `aes-internal.c`,
  `aes-internal-enc.c` and `aes_i.h`;
- `include/osmocom/crypt/auth.h`;
- `tests/auth/milenage_test.c` and `tests/auth/milenage_test.ok`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. `include/osmocom/core/bit64gen.h` is generated
by the build, so it isn't pinned.

The harness rebuilds `milenage_test.ok` from its own calls and fails if a line differs.
Test set 1's recorded outputs also match the values 3GPP TS 35.207 publishes.

## Checking

```bash
./reference/auth/run_auth_vector.sh | cmp - tests/vectors/auth/auth_vectors.ndjson
```
