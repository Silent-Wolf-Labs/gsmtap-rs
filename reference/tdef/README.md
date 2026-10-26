# TDef reference

`tdef_vector.c` calls the core `osmo_tdef_*` API: lookup and mutation, unit
conversion, reset, sentinel traversal, range checks, value formatting, unit
names and state-timeout tables. FSM integration is recorded separately in
[`fsm/`](../fsm/README.md).

## Fixtures

| File | Contents |
| --- | --- |
| `tests/vectors/tdef/tdef_vectors.ndjson` | 2,175 golden records |
| `tests/vectors/tdef/tdef_diagnostics.json` | 95 isolated probes |

The golden records are 1 metadata record, 900 supported unit conversion get and
set pairs, 30 safe invalid-unit pairs, 81 zero-valued unit pairs, 7 lookup and
mutation traces, 1 reset trace, 14 missing-table cases, 4 sentinel and null
traversal cases, 360 range and set cases, 34 valid reset boundaries, 408
complete format buffers, 9 unit names, 6 unit-name table entries including the
terminator and 320 state-timeout observations. `osmo_tdef_get_entry()` is
called directly for signed and zero IDs, duplicate first matches, missing IDs
and entries hidden after the sentinel.

The probes are 3 invalid-default panic snapshots, 3 real assertion aborts, 88
sanitizer-diagnosed divisions by zero (44 invalid pairs through get and set)
and one null-buffer write. They are never golden results.

## Recorded C behavior

- Formatting with a declared capacity of zero still writes `buf[0]`. The
  harness backs it with a 64-byte array, so the write is valid and recorded.
- Conversions between unknown or identical custom units, zero values and
  missing-entry fallbacks with unknown units are defined and recorded.
- Setting a missing entry returns `-EEXIST` (−17), and a rejected value returns
  `-ERANGE` (−34).

## Excluded cases

Reset snapshots use the supported panic handler and exit with status 77
without returning from it. Ordinary negative-fallback and invalid-state probes
use the default panic path and require `SIGABRT`. Each probe is bounded to five
seconds with core dumps disabled. Unstable addresses, backtraces and executable
paths are not recorded.

## Provenance

The runner builds the unmodified `tdef.c`, `utils.c`, `panic.c` and
`backtrace.c`. Link-time optimization and section garbage collection drop the
unused FSM and logging paths, so nothing is stubbed. `tdef_provenance.json`
pins 24 local files and 106 platform headers. The revision is `unknown`. The
recorded ABI has 64-bit `unsigned long` and `long` and 32-bit `int`; 32-bit
`ULONG_MAX` behavior is not covered.

## Checking

```bash
./reference/tdef/run_tdef_vector.sh --verify
./reference/tdef/run_tdef_vector.sh --audit
./reference/tdef/run_tdef_vector.sh all | cmp - tests/vectors/tdef/tdef_vectors.ndjson
./reference/tdef/run_tdef_vector.sh --diagnostics | cmp - tests/vectors/tdef/tdef_diagnostics.json
```
