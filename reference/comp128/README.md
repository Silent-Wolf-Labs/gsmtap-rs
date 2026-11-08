# COMP128 and XOR-2G reference

`comp128_vector.c` calls the public API directly and records:

- SRES and Kc from COMP128 v1, v2 and v3 for the 1,024 v2 and 1,024 v3 vectors in
  upstream's `comp128_test.c`;
- the same for each version over a sweep: every pair of five fixed Ki and RAND patterns,
  each Ki bit alone, each RAND bit alone, and 64 random pairs;
- SRES and Kc from XOR-2G through `osmo_auth_gen_vec2()`, over the same sweep with 16
  random pairs.

For every COMP128 record the harness also checks that `osmo_auth_gen_vec2()` gives the
same SRES and Kc as the direct call. For every call to `osmo_auth_gen_vec2()` it checks
that the call copies RAND, sets the GSM type and leaves the UMTS fields alone.

libosmogsm doesn't export `comp128v1()`, so v1 is called through the deprecated
`comp128()`, which only calls it. `comp128v2()` and `comp128v3()` always return 0, and the
harness fails if they don't.

Not recorded: `osmo_auth_gen_vec2()` with a UMTS subscriber for these algorithms. C reads
the GSM Ki from the same memory as the UMTS fields, so the result says nothing about the
algorithm. XOR-2G's `-ENOTSUP` for a non-GSM subscriber, the deprecated
`osmo_auth_gen_vec()` and the rest of the auth core come with the auth core harness.

## Fixtures

`tests/vectors/comp128/comp128_vectors.ndjson` starts with a metadata line, followed by one
section per kind of record. `tests/support/comp128_fixture.rs` pins an FNV-1a fingerprint of
the file and checks:
- the sections and fields;
- that upstream vectors are v2 or v3 with upstream's Ki;
- that v1 and v2 give a Kc whose last 10 bits are zero;
- that XOR-2G's SRES and Kc are the first 12 bytes of Ki XOR RAND.

## Provenance

The runner checks the hashes of:
- `src/gsm/comp128.c`, `src/gsm/comp128v23.c`;
- `src/gsm/auth_comp128v1.c`, `src/gsm/auth_comp128v23.c`, `src/gsm/auth_xor_2g.c`,
  `src/gsm/auth_core.c`;
- `include/osmocom/gsm/comp128.h`, `include/osmocom/gsm/comp128v23.h`,
  `include/osmocom/crypt/auth.h`;
- `tests/comp128/comp128_test.c`, `tests/comp128/comp128_test.ok`, `tests/auth/xor2g_test.c`
  and `tests/auth/xor2g_test.ok`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

The harness reads every vector from `comp128_test.c`, and fails if one differs. It
rebuilds `comp128_test.ok` and `xor2g_test.ok` from its own calls and fails if a line
differs.

## Checking

```bash
./reference/comp128/run_comp128_vector.sh | cmp - tests/vectors/comp128/comp128_vectors.ndjson
```
