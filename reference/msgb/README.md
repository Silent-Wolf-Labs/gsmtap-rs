# Message-buffer reference

Four harnesses call the `msgb_*` API directly, including the inline helpers in
`msgb.h`. `run_msgb_vector.sh` selects one with a mode flag. The harnesses
share `msgb_vector_common.h`.

Every record snapshots complete before and after state: all backing bytes,
data and tail offsets, length and capacity, the four layer markers, the control
words, destination and channel presence, the query results (data, length,
headlen, headroom, tailroom and the invariant check) and a normalized panic or
error outcome. Head is always zero. Pointers are recorded as offsets, or `-1`
for null; destination and channel record presence, never addresses.

## Fixtures

| Mode | Harness | Fixture in `tests/vectors/msgb/` | Records |
| --- | --- | --- | ---: |
| (none) | `msgb_vector.c` | `msgb_vectors.json` | 3,695 |
| `--layers` | `msgb_layers_vector.c` | `msgb_layers_vectors.json` | 7,572 |
| `--resize` | `msgb_resize_vector.c` | `msgb_resize_vectors.json` | 8,888 |
| `--adapters` | `msgb_adapters_vector.c` | `msgb_adapters_vectors.json` | 4,055 |

Record counts exclude setup calls and snapshot queries. Each fixture has a
reader in `tests/support/msgb_*fixture.rs`; the later readers build on
`msgb_fixture.rs`. The adapters reader also pins an FNV-1a fingerprint of the
whole file.

## Coverage

- **Storage:** data and length, headlen, headroom and tailroom, reserve, put
  and get, push and pull, trim, reset, the invariant check and construction.
  Every offset and length for capacities 0, 1 and 2; selected offsets for 8
  and 16; zero, exact and excess arguments; nonzero headroom; all
  optional-layer combinations; reset, reuse sequences and the 65,535-byte
  capacity.
- **Typed and layer helpers:** `msgb_put/push/get/pull_u8/u16/u32`,
  `msgb_l1` to `msgb_l4`, `msgb_sms`, `msgb_l1len` to `msgb_l4len`,
  `msgb_pull_to_l2`, `msgb_pull_to_l3`, `msgb_l3trim` and `msgb_push_tl`. All
  256 values for the u8 operations, u16 and u32 boundaries, layer markers in
  headroom, data and tailroom or unset, and a 14-step mixed sequence.
- **Copy and resize:** 1,131 calls to `msgb_copy`, `msgb_copy_c`,
  `msgb_copy_resize` and `msgb_copy_resize_c`, and 7,757 calls to
  `msgb_resize_area`. Copies record the unchanged source and the complete new
  buffer, with pointer identity reduced to a distinct-storage flag.
- **Adapters:** 450 slice comparisons including NULL inputs, 270 message
  comparisons, 2,942 caller-buffer hex dumps, 62 static and 62 allocated dumps,
  152 layer dumps, 90 `msgb_printf` appends and 27 queue operations.

## Recorded C behavior

- Typed multi-byte operations are big-endian. Typed get and pull leave the
  removed bytes in storage.
- Layer lengths use unsigned wrapping subtraction when a marker is past tail.
- `msgb_l3trim` sets the length to `(L3 - data) + l3len` in signed arithmetic;
  negative `l3len` is accepted when the total stays nonnegative.
- `msgb_push_tl` writes the tag and the previous length narrowed to 8 bits.
- Reset keeps L1 and the bytes; it clears L2 to L4 and the control words.
- Copies keep only the active bytes; headroom and unused tailroom in the new
  buffer are zero. Control words and destination and channel are cleared.
- `msgb_resize_area` moves the suffix with memmove semantics. Markers at or
  after the end of the old area move; markers inside it stay put, even when
  that leaves them past tail or out of order. Growth does not initialize the
  extension, and shrinking does not erase the unused suffix.
- Hex dumps keep the 1365-byte input limit per segment and annotate layer
  markers in headroom, out of order or past tail.
- `msgb_printf` needs a spare NUL byte outside the active data. When space runs
  out it returns `-EINVAL` but still writes truncated output and a NUL into the
  unused tail.
- Counted queue operations wrap their `unsigned int` counter; an empty dequeue
  leaves it unchanged.

## Excluded cases

Trim or growth that relies on C's incomplete headroom check, reserve on a
nonempty message, invalid offsets,
NULL access, pointer arithmetic beyond an allocation, NULL relational
comparisons during resize, unchecked `snprintf` truncation in dump annotations
and implementation-defined signed narrowing are never cases. Successful u16
and u32 pushes use aligned destinations, because the C helpers cast to typed
pointers. Talloc contexts, names and global talloc state are not recorded.

## Provenance

The runner checks the hashes of `msgb.c`, `msgb.h`, `msgb_test.c`, `panic.c`
and `panic.h`. Each mode adds its own dependencies:

- `--layers`: `bits.h`, `utils.h`, the `bitXXgen.h.tpl` template, the
  `Makefile.am` rule that generates headers from it, and the generated
  `bit16gen.h` and `bit32gen.h`.
- `--resize`: `logging.c` and `logging.h`. Logging is initialized without
  targets so copy-failure logging does not panic.
- `--adapters`: `utils.c`, `utils.h` and `linuxlist.h`. The queue harness is
  built as GNU C11, because the list macros use `typeof`.

All were verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`, and the two generated headers were
reproduced from the template. The recorded ABI is 8-bit bytes, 32-bit `int`
and `unsigned int`, and 64-bit `unsigned long` and pointers. Panic cases use a
controlled `setjmp`/`longjmp` handler, as the upstream tests do.

## Checking

```bash
./reference/msgb/run_msgb_vector.sh | cmp - tests/vectors/msgb/msgb_vectors.json
./reference/msgb/run_msgb_vector.sh --layers | cmp - tests/vectors/msgb/msgb_layers_vectors.json
./reference/msgb/run_msgb_vector.sh --resize | cmp - tests/vectors/msgb/msgb_resize_vectors.json
./reference/msgb/run_msgb_vector.sh --adapters | cmp - tests/vectors/msgb/msgb_adapters_vectors.json
```

When intentionally changing the adapter cases, update the reader's frozen input
manifest, count and fingerprint from the reviewed C output.
