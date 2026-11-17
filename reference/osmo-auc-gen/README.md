# osmo-auc-gen reference

`run_osmo_auc_gen_vector.sh` builds libosmocore's `osmo-auc-gen` utility from the
checkout's `utils/osmo-auc-gen.c`, linked against the checkout's `libosmogsm` and
`libosmocore`. It runs upstream's `tests/osmo-auc-gen/osmo-auc-gen_test.sh` against that
build and fails unless its stdout and stderr equal the pinned `osmo-auc-gen_test.ok` and
`osmo-auc-gen_test.err`.

`osmo_auc_gen_vector.c` then runs the program once per case, with `osmo-auc-gen` as its
`argv[0]`, and records its exit status, stdout and stderr. It runs each case twice and fails
if the runs differ. It also rebuilds the test script's expected output from its upstream
records and fails if that differs from the `.ok` and `.err` files. The cases are:

- every command of `osmo-auc-gen_test.sh`, in order: Milenage with SQN, and resync from
  AUTS with IND slots and IND lengths, including the four IND errors;
- COMP128v1, v2 and v3, XOR-2G and XOR-3G as GSM algorithms, the `triplets.dat` output, and
  Milenage and TUAK given `-2`, which fail;
- Milenage with OP, AMF, each RES length, SQN as hex, octal, negative, past 48 bits, past
  64 bits and text, and IND lengths up to 63;
- XOR-3G and TUAK as UMTS algorithms, with resync, wrong key and OPc lengths, and failed
  AUTS;
- key, OPc, RAND, AMF and AUTS arguments of the wrong length, with odd digits, with
  whitespace and with non-hex characters, and options that only UMTS takes given with `-2`;
- long, abbreviated, `=` and grouped option forms, positional arguments, unknown and
  ambiguous options, missing arguments, `--help`, and missing `-2`, `-3` or algorithm.

Every case gives a RAND with `-r`, or stops before generating one. The program draws a
random RAND otherwise, so those runs can't be recorded.

## Behaviour the records show

The program sets keys in `struct osmo_sub_auth_data2`, whose GSM Ki shares a union with the
UMTS fields. Milenage and TUAK given `-2` therefore see a zero K length and fail. A GSM
algorithm given `-3` reads its Ki from the first 16 bytes of the OPc buffer, and `-k`, `-o`
or `-O` given before a later `-2` or `-3` lands in whichever union member that option
wrote. No case records those combinations, since their results depend only on that
layout. A `-k` before any `-2` or `-3` is dropped with a message, and the case that shows
this sets no other key.

`-s` is parsed with `strtoull()` in base 0, and `-i`, `-l` and `-L` with `atoi()`.
`--ind -1` is stored as 4294967295. An AUTS shorter than 14 bytes is padded with zeros. A
`-3` resets the IND length to 5 even after `-l`.

Shifts by the IND length are undefined in C from 31 bits for `--ind` and from 64 bits
without it, so no case uses those. Lengths above 28 fail in the auth core.

Option errors print glibc's `getopt_long()` messages, prefixed with the program name; the
records were made with glibc 2.39. Help goes to stdout, apart from the list of algorithms,
which goes to stderr.

## Fixtures

`tests/vectors/osmo-auc-gen/osmo_auc_gen_vectors.ndjson` starts with a metadata line,
followed by one record per run: the upstream cases, then the harness's.
`tests/support/osmo_auc_gen_fixture.rs` pins an FNV-1a fingerprint of the file and checks:
- the sections, field order and string escapes;
- unique kebab-case case names;
- exit statuses from 0 to 2;
- that every stdout starts with the same banner, ending at the first empty line.

## Provenance

The runner checks the hashes of:
- `utils/osmo-auc-gen.c`;
- `tests/osmo-auc-gen/osmo-auc-gen_test.sh`, `osmo-auc-gen_test.ok` and
  `osmo-auc-gen_test.err`;
- `src/gsm/auth_core.c`, `auth_milenage.c`, `auth_tuak.c`, `auth_xor.c`, `auth_xor_2g.c`,
  `auth_comp128v1.c` and `auth_comp128v23.c`;
- `include/osmocom/crypt/auth.h`;
- `src/core/utils.c` and `src/core/base64.c`.

Each was verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. The four `osmo-auc-gen` files were downloaded
from the GitHub mirror of libosmocore at that revision, and the others were verified for
the auth, tuak, comp128, hexparse and base64 runners.

The TUAK and XOR-3G resync cases take their key, RAND and AUTS from inputs of the tuak and
auth harnesses that C accepts. The program's output for them comes from these runs.

## Checking

```bash
./reference/osmo-auc-gen/run_osmo_auc_gen_vector.sh | cmp - tests/vectors/osmo-auc-gen/osmo_auc_gen_vectors.ndjson
```
