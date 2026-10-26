# FSM reference

Five harnesses call the `osmo_fsm_*` API directly, each recording one area. Each
has its own runner, manifests and fixture directory, `tests/vectors/<family>/`.

| Family | Records | Scenarios | Probes | Covers |
| --- | ---: | ---: | ---: | --- |
| `fsm_core` | 1,506 | 64 | 7 | Standalone execution and termination, without parents or children |
| `fsm_timer` | 9,369 | 1,080 | 5 | State and timer changes from callbacks, failed transitions with active timers for all five state-change forms, timeout return values and the TDef bridge |
| `fsm_tree` | 2,010 | 134 | 5 | Lookup visibility, hierarchy membership, cleanup and parent-notification order, including safe and legacy termination and nested cleanup |
| `fsm_registry` | 1,524 | 127 | 2 | ID and name lookup, identifier validation and sanitizing, rejected updates, duplicate order, slot reuse, nullable state names, unregistering and allocating afterward |
| `fsm_shared` | 443 | 21 | 0 | FSM and application timers on one scheduler: simultaneous expiry, peer mutation, timers created in callbacks and TDef changes |

Each directory holds `<family>_vectors.tsv` (a metadata line plus the records),
`<family>_diagnostics.json` and copies of both manifests.
`tests/support/fsm_fixture.rs` checks chronology, canonical numbers and hex
strings, record shape, frozen observations and manifests and corruption
rejection. `tests/fsm_<family>_reference_vectors.rs` pins each family's
fingerprints and manifest digests.

## Record layout

- **Core and timer:** scenario, step, operation, argument, status, then the
  instance's state, T value, pending flag, deadline, terminating flag, ID
  lookup, name lookup and instance count. Timer records add four operation
  inputs and the wall-clock value.
- **Tree and shared:** scenario, step, operation, target, argument and status,
  then four complete instance snapshots (state, T, terminating, visibility,
  parent index, children mask, pending, deadline seconds and microseconds) and
  the instance list order.
- **Registry:** the query and name-helper inputs and outputs, registration and
  list order, and three snapshots of state, T, visibility, ID and name.

`-1` marks an absent instance or index. `-2` marks an unavailable pointer
identity, or an ID lookup skipped because C would dereference a null ID. `s:`
prefixes lowercase hexadecimal string bytes, and `s:-` is null.

## Recorded C behavior

- Allocation still succeeds after the FSM is unregistered, although the C
  comment says otherwise.

## Excluded cases

Unsafe cases run as bounded isolated subprocesses and are recorded only as
diagnostics:

- `fsm_core`: event and state numbers 31 and 32, which shift out of range, null
  state-change and termination calls, and an invalid current state that must
  assert.
- `fsm_timer`: an invalid TDef unit, fallback overflow and a null timeout map,
  which are undefined behavior, and a missing TDef entry and a TDef state change
  to state 32, past the end of the timeout map, which must assert.
- `fsm_tree`: four use-after-free cases (legacy cleanup, freeing linked
  instances, child-to-parent notification and parent broadcast) and a null
  parent.
- `fsm_registry`: null stored and null queried IDs.

Logging, VTY, CTRL, native address names, arbitrary talloc ownership and
destructors, select and timerfd, and statistics are not recorded.

## Provenance

Each runner compiles the unmodified local FSM, TDef, timer, utility, logging
and support sources with the real libtalloc. The manifests pin 39 project files
and 149 platform dependencies including libtalloc, the compiler version and
target. The runner checks every transitive include, clears search-path
injection variables and selects the checked libtalloc directory at run time.
Logging is set up once per process with no targets. The revision is `unknown`.

## Checking

```bash
for family in fsm_core fsm_timer fsm_tree fsm_registry fsm_shared; do
    runner="./reference/fsm/run_${family}_vector.sh"
    "$runner" --verify
    "$runner" --audit
    "$runner" all | cmp - "tests/vectors/${family}/${family}_vectors.tsv"
    "$runner" --diagnostics | cmp - "tests/vectors/${family}/${family}_diagnostics.json"
done
```
