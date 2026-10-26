# time_cc reference

`time_cc_vector.c` calls the `osmo_time_cc_*` API, which accumulates how long a
flag was set and reports it to a rate counter. It uses the real timer, TDef and
`rate_ctr_add()` against initialized `struct rate_ctr` storage, and checks
that the counter's previous and interval fields are left untouched.

## Fixtures

| File | Contents |
| --- | --- |
| `tests/vectors/time_cc/time_cc_vectors.ndjson` | 1 metadata record and 5,947 trace records in 469 scenarios |
| `tests/vectors/time_cc/time_cc_diagnostics.json` | 5 isolated probes |

448 scenarios form a matrix of eight granularity values, seven thresholds, four
forgetting values and a present or absent counter. The other 21 isolate clock
skew and delay, cleanup and reuse, zero start, the `EIO` fallback,
configuration and TDef changes, a missing counter, wrapping, report caps and the
`UINT64_MAX` next-event branches. `seed` operations set the public accounting
fields to boundary states; they are edge probes, not normal histories.

Each trace records the internal sums, configuration and counter, active
status, the full deadline, the registered timer count and the dispatch work.

The probes cover null init, set and cleanup, division by zero through an
invalid TDef unit and the missing-TDef assertion.

## Recorded C behavior

- Unsigned time and counter arithmetic wraps modulo 2^64.
- Timer-relative seconds narrow to the low 32 bits of a signed `int`. This is
  implementation-defined GCC behavior on the recorded target.
- A monotonic clock failure (`EIO`) is a defined zero-timestamp outcome and is
  part of the successful traces.

## Excluded cases

Each probe runs in its own process for at most five seconds. Assertions must
raise `SIGABRT`, and undefined-behavior cases must produce a sanitizer
diagnostic. The successful traces must be ASan and UBSan clean, including the
intentional wrapping edges. Wall-clock failures during timer scheduling have no
C result to record, because the return value is ignored.

## Provenance

The runner compiles the unmodified `time_cc.c`, `timer.c`, both clock shims,
rbtree, TDef, rate counter, utils, panic and backtrace sources as GNU C11 with
link-time optimization and section garbage collection, so no FSM, logging,
select, rate-group or talloc stubs are needed. The only fault injection wraps
libc `clock_gettime()` to return `EIO` while the public monotonic override is
disabled; no libosmocore function is replaced.

`time_cc_provenance.json` pins 30 local files and 128 platform headers. The
revision is `unknown`. The recorded ABI has 64-bit `unsigned long`, `long` and
time fields, 32-bit `int` and 8-bit bytes.

## Checking

```bash
./reference/time_cc/run_time_cc_vector.sh --verify
./reference/time_cc/run_time_cc_vector.sh --audit
./reference/time_cc/run_time_cc_vector.sh all | cmp - tests/vectors/time_cc/time_cc_vectors.ndjson
./reference/time_cc/run_time_cc_vector.sh --diagnostics | cmp - tests/vectors/time_cc/time_cc_diagnostics.json
```
