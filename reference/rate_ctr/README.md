# Rate-counter reference

Two harnesses record libosmocore rate counters:

- `rate_ctr_vector.c` calls the standalone counter operations: add, inline
  increment, difference and reset.
- `rate_ctr_group_vector.c` calls the group, registry, lookup, reset and
  iteration APIs and the interval rollup. It includes the unmodified
  `src/core/rate_ctr.c` to reach its static timer callback, and delivers
  expiration counts through pipe reads.

## Fixtures

| File | Contents |
| --- | --- |
| `tests/vectors/rate_ctr/rate_ctr_vectors.tsv` | 1 metadata line and 1,700 calls in 100 scenarios |
| `tests/vectors/rate_ctr/rate_ctr_diagnostics.json` | 4 null-pointer probes |
| `tests/vectors/rate_ctr_group/rate_ctr_group_vectors.tsv` | 1 metadata line and 1,915 records in 608 scenarios |
| `tests/vectors/rate_ctr_group/rate_ctr_group_native.tsv` | 3 native initialization records |
| `tests/vectors/rate_ctr_group/rate_ctr_group_diagnostics.json` | 7 isolated probes |

### Standalone counters

Each line holds the scenario, step, operation, signed increment, signed return
(0 for void calls) and ten state words before and after: current, previous and
the last and rate pairs for second, minute, hour and day. Initial current and
previous values form a 10 by 10 boundary matrix, and each scenario runs the
same 17 calls: difference twice, seven signed additions, increment, difference
twice, reset, difference, add −1, difference and reset. Values include zero,
`INT_MIN`, `INT_MAX`, `INT64_MAX`, high-bit values and `UINT64_MAX`.

### Groups

The group records preserve group order, effective descriptor metadata, indices
and classes, optional report names, the callback trace and return, the tick
phase and every current, previous and interval word before and after each
operation. They cover full group and counter identifier byte matrices,
duplicate-index wrapping and name mangling, lifecycle and lookup, reset during
iteration, nested lookup and iteration, mutation, removal and allocation of the
current and next group, interval boundaries, tick and counter wrap, and delayed
catch-up. Four records show descriptor aliasing and copied report names.

The native records run the real select and timerfd initialization with
failures injected at the OS boundary, each in its own process. A bounded
one-second select dispatch also runs as a smoke check.

## Recorded C behavior

- Counter arithmetic wraps, and the unsigned-to-`int64` conversion is
  implementation-defined; it is recorded for GCC on 64-bit Linux.
- Freeing the next group, and freeing the current group from a callback that
  returns a negative value, are defined and part of the successful records.

## Excluded cases

The standalone probes are null add, increment, difference and reset. The group
probes are continuing after freeing or unlinking the current group,
out-of-bounds counter access, a null descriptor, a short expiration read, a
zero expiration count and reinitializing the current unlinked list node, which
never terminates and is bounded. All successful records are ASan and UBSan
clean.

## Provenance

The standalone runner compiles only `rate_ctr.c` with the harness; link-time
optimization and section garbage collection drop the registry, logging and
timer dependencies, and no observed API is stubbed. `rate_ctr_provenance.json`
pins 14 local files and 124 platform headers.

The group runner pins 47 local sources and headers, 227 platform headers and
libraries, the compiler identity and the exact normal, audit and link flags.
`rate_ctr_group_preprocessor_defines.txt` holds the preprocessor defines
(`cc -dM -E` on the harness with the pinned flags). It is pinned in the
generator manifest and recomputed on every run. The build includes the
stats, stat-item, socket and io_uring dependencies of select.

Both revisions are `unknown`.

## Checking

```bash
./reference/rate_ctr/run_rate_ctr_vector.sh --verify
./reference/rate_ctr/run_rate_ctr_vector.sh --audit
./reference/rate_ctr/run_rate_ctr_vector.sh all | cmp - tests/vectors/rate_ctr/rate_ctr_vectors.tsv
./reference/rate_ctr/run_rate_ctr_vector.sh --diagnostics | cmp - tests/vectors/rate_ctr/rate_ctr_diagnostics.json

./reference/rate_ctr/run_rate_ctr_group_vector.sh --verify
./reference/rate_ctr/run_rate_ctr_group_vector.sh --audit
./reference/rate_ctr/run_rate_ctr_group_vector.sh all | cmp - tests/vectors/rate_ctr_group/rate_ctr_group_vectors.tsv
./reference/rate_ctr/run_rate_ctr_group_vector.sh --diagnostics | cmp - tests/vectors/rate_ctr_group/rate_ctr_group_diagnostics.json
./reference/rate_ctr/run_rate_ctr_group_vector.sh --native | cmp - tests/vectors/rate_ctr_group/rate_ctr_group_native.tsv
```

For a change to the group harness, flags or build inputs, recompute the defines
file with the pinned flags, then update its digest, the source manifest and the
generator manifest in that order.
