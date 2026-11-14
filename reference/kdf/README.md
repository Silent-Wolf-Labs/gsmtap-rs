# Key derivation reference

`kdf_vector.c` calls libosmogsm's key derivation functions directly. Each is an
HMAC-SHA-256 over the parameters 3GPP TS 33.220 Annex B lays out. libosmocore has no test
for them, so the harness records:

- `osmo_kdf_kc128()` (3GPP TS 33.102) for fixed patterns, CK and IK swapped, and random
  keys;
- `osmo_kdf_kasme()` (3GPP TS 33.401) for fixed patterns, two PLMN identities, one
  concealed SQN given three ways, an SQN equal to its AK, and random inputs;
- `osmo_kdf_enb()` for the edges of the 32-bit uplink NAS count and random inputs;
- `osmo_kdf_nh()` for fixed patterns, a chain from K_eNB through three NHs, as TS 33.401
  section 7.2.8 describes, and random inputs;
- `osmo_kdf_nas()` for every algorithm type from 0 to 7 with every identity from 0 to 15,
  the edges of both bytes, and random keys.

C takes any byte for the NAS algorithm type and identity, although TS 33.401 defines only
types 1 to 6 and four-bit identities, so the sweep records values past both.

Each record holds its inputs, then the output followed by four guard bytes of `0x55`; the
harness fails if a function writes past its output. The pseudo-random inputs come from a
fixed-seed xorshift generator, so the fixture is reproducible.

The functions are exported from `libosmogsm.so`, which the runner builds and links. In
the reference build, libosmogsm computes the HMAC with GnuTLS; libosmocore's own SHA-256
in `src/gsm/kdf/` is used only when it's built without GnuTLS.

## Fixtures

`tests/vectors/kdf/kdf_vectors.ndjson` starts with a metadata line, followed by one
section per function. `tests/support/kdf_fixture.rs` pins an FNV-1a fingerprint of the
file and checks:
- the sections, fields and sources;
- the range of each integer;
- each output's length and guard bytes.

## Provenance

The runner checks the hashes of `src/gsm/kdf.c` and `include/osmocom/crypt/kdf.h`. Each
was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

## Checking

```bash
./reference/kdf/run_kdf_vector.sh | cmp - tests/vectors/kdf/kdf_vectors.ndjson
```
