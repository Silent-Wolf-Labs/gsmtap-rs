# Time and timer reference

`time_vector.c` records the clock, timeval and timespec helpers and the timer
API (`osmo_timer_*`). It uses the public clock overrides for deterministic
time, and never sleeps.

## Fixtures

`tests/vectors/time/time_vectors.ndjson` holds 20,933 records:

| Kind | Records |
| --- | ---: |
| Metadata | 1 |
| Raw timeval and timespec operations | 20,736 |
| Clock | 44 |
| Seconds helpers | 48 |
| Timer traces | 72 |
| Timer API | 32 |

Fractional fields span and exceed the one and two-second boundaries in both
directions. Operations include in-place aliases and the override add.
`tests/support/time_fixture.rs` freezes the schema, ordered coverage,
provenance and complete observations.

## Coverage

The timer traces include self and peer mutation from callbacks, ordinary
deferred immediate timers, a callback that calls a nested update, a callback
that adds an immediate timer and then nests an update, and prepare or nearest
during dispatch. They also record callback replacement, remaining-time
equality and raw boundaries, relative scheduling, nearest-time rounding up to
`INT_MAX`, absolute lifecycle and caching, missing callbacks and `LONG_MAX`
dispatch.

## Recorded C behavior

- A `LONG_MAX` deadline suppresses only the nearest-time cache, not activation
  or dispatch.
- Nested updates from a callback are recorded as C behaves. A separate bounded
  probe, never fixture data, shows a nested peer update re-running the outer
  callback; it exits with status 77 after 12 callbacks. osmocore-rs rejects
  nested updates instead, so its replay skips the 15 recursive trace records.

## Excluded cases

`--audit` runs the golden calls and the nested-peer probe under ASan and UBSan,
then requires signed-overflow diagnostics for seconds add and subtract, carry,
borrow and fraction addition. Those diagnostics are never expected outputs.

## Provenance

`time_provenance.json` pins 13 participating source, header, configuration,
build and test files and 94 platform headers. The revision is `unknown`. The
evidence is for 64-bit Linux, not an embedded build.

## Checking

```bash
./reference/time/run_time_vector.sh --verify
./reference/time/run_time_vector.sh --audit
./reference/time/run_time_vector.sh all | cmp - tests/vectors/time/time_vectors.ndjson
```

The runner withholds stdout until every probe passes.
