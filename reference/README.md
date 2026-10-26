# libosmocore reference harnesses

Each directory here holds direct-call C harnesses for one libosmocore module.
A harness calls public libosmocore APIs and records return values, complete
buffer contents and callback traces. Those recordings, committed under
`tests/vectors/`, are the behavioral specification:

1. **Recorded outputs define behavior.** Expected results come from direct C
   calls only, never from Rust or from a reimplementation inside a harness.
2. **The C source was consulted** to understand those results and decide what
   carries over to Rust.
3. **osmocore-rs is idiomatic Rust** verified against the fixtures. No C code,
   comments or data tables were copied.

gsmtap-rs validates the fixtures' structure, provenance and integrity.
osmocore-rs compares its implementation against the same records in its own
compatibility tests.

## Modules

| Module | Harness | Fixtures | APIs recorded |
| --- | --- | --- | --- |
| GSMTAP | [`gsmtap/`](gsmtap/README.md) | `tests/vectors/*.json` | `gsmtap_makemsg_ex`, `gsmtap_makemsg` |
| BCD | [`bcd/`](bcd/README.md) | `tests/vectors/bcd/` | BCD digit and string conversion |
| Hexparse | [`hexparse/`](hexparse/README.md) | `tests/vectors/hexparse/` | `osmo_hexparse` |
| Bit packing | [`bits/`](bits/README.md) | `tests/vectors/bits/` | Packed and unpacked bit conversion |
| Base64 | [`base64/`](base64/README.md) | `tests/vectors/base64/` | Base64 encode and decode |
| Bit vectors | [`bitvec/`](bitvec/README.md) | `tests/vectors/bitvec/` | `bitvec_*` |
| Message buffers | [`msgb/`](msgb/README.md) | `tests/vectors/msgb/` | `msgb_*` |
| Primitives | [`prim/`](prim/README.md) | `tests/vectors/prim/` | `osmo_prim_*` |
| CRC | [`crc/`](crc/README.md) | `tests/vectors/crc/` | CRC-16/CCITT and generic CRC 8/16/32/64 |
| Convolutional codes | [`conv/`](conv/README.md) | `tests/vectors/conv/` | `osmo_conv_*` |
| Time and timers | [`time/`](time/README.md) | `tests/vectors/time/` | Clock, timeval/timespec and `osmo_timer_*` |
| TDef | [`tdef/`](tdef/README.md) | `tests/vectors/tdef/` | `osmo_tdef_*` |
| time_cc | [`time_cc/`](time_cc/README.md) | `tests/vectors/time_cc/` | `osmo_time_cc_*` |
| Rate counters | [`rate_ctr/`](rate_ctr/README.md) | `tests/vectors/rate_ctr/`, `tests/vectors/rate_ctr_group/` | Counters, groups and interval rollups |
| FSM | [`fsm/`](fsm/README.md) | `tests/vectors/fsm_*/` | `osmo_fsm_*` |
| Networking | [`network/`](network/README.md) | `tests/vectors/net/` | `osmo_sock_init2`, `osmo_sock_set_dscp` |

## The C checkout

Every runner builds against the libosmocore checkout at
`../libosmocore-rs/libosmocore`, relative to this repository. The checkout is
intentionally untracked and has no `.git`. Runners fail if it is missing or a
pinned file differs; they never fall back to another checkout or a system
installation. Do not modify it.

Never take a revision from a parent repository. Where a module names upstream
revision `950430e829a3dc1d162aa241bc0505745c5a7311`, its pinned files were
downloaded individually from that revision and compared byte-for-byte with the
local checkout. Elsewhere the revision is recorded as `unknown`.

For other tools that need an installed library, source
`scripts/libosmocore-env.sh` in Bash. It selects this checkout's `_install/`
directory and rejects `LIBOSMOCORE_ROOT` values that point elsewhere.

## Two kinds of runner

**Library runners** (gsmtap, bcd, hexparse, bits, base64, bitvec, msgb, prim,
crc, network) run `make` in the checkout's `src/core` and link the harness
against that library with an rpath. All except gsmtap first check SHA-256
hashes of the C sources and headers they exercise. The bits, base64, bitvec,
msgb, prim, crc and network runners also replace `LD_LIBRARY_PATH` and clear
`LD_PRELOAD` at run time. Their fixtures are JSON or TSV.

**Exact-source runners** (conv, time, tdef, time_cc, rate_ctr, fsm) never
rebuild the checkout. They compile the exact participating sources into
temporary executables. Each family has two manifests, which are also copied
next to its fixtures:

- `<family>_provenance.json` pins every participating source and header and
  the platform headers, and for most families the compiler too. The runner
  checks every transitive include against it.
- `<family>_generator_provenance.json` pins the harness, the runner and the
  digest of the source manifest.

Fixture metadata records both manifest digests, and the readers pin them. The
fixtures are NDJSON or TSV with a leading metadata record. These runners share
a set of modes:

| Mode | Output |
| --- | --- |
| `all` (default) | Golden observations on stdout. |
| `--verify` | Checks the manifests and exits without writing observations. |
| `--audit` | Reruns the golden domain under ASan and UBSan, then requires the expected diagnostics from the excluded unsafe cases. LSan is disabled under ptrace. |
| `--diagnostics` | Isolated unsafe probes as JSON. These are never golden results. |

Unsafe C calls, such as undefined shifts, null pointers and out-of-bounds
access, are never golden cases. Where a module needs evidence about them, it
records separate diagnostics. Rust safety behavior for those calls is tested in
osmocore-rs.

## Regenerating and checking

From this repository root:

```bash
./scripts/generate-reference-vectors.sh
git status tests/vectors
./scripts/compare-shared-fixtures.sh
```

The generator runs every runner except network, publishes each family only
after all of its outputs succeed and copies the exact-source manifests next to
their fixtures. A clean `git status` means the C output still matches the
committed fixtures. `compare-shared-fixtures.sh` checks that every fixture,
reader and manifest matches its osmocore-rs copy, and that the manifests next
to the fixtures match the ones here. Each module's README gives the commands
for checking only that module.

Ordinary `cargo test` reads the committed fixtures and needs no C build.

## Changing fixtures or runners

Review every regeneration diff, including unrelated existing fixtures, and
explain any changed byte before accepting it. Case names, inputs, order and
counts are frozen in the readers; changing them is a fixture-contract change.

For an exact-source family, editing the harness or runner changes its hash. To
carry that through:

1. Update the hash in `<family>_generator_provenance.json`. A source or build
   input change goes into `<family>_provenance.json` first, and then its new
   digest into the generator manifest.
2. Regenerate with the generator.
3. Update the manifest digests and FNV-1a fingerprints that the readers pin, in
   `tests/support/` and in the FSM tests, then run `cargo test`.
4. Copy the changed fixtures and readers to osmocore-rs, update the
   osmocore-rs-only tests that pin the same values, then run
   `./scripts/compare-shared-fixtures.sh` and the osmocore-rs test suite.

The FNV-1a fingerprints check fixture integrity. They are not a cryptographic
authenticity claim.
