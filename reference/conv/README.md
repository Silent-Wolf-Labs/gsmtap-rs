# Convolutional-code reference

`conv_vector.c` calls the 17 selected core `osmo_conv_*` exports directly:
descriptor helpers, reusable and one-shot encoding, the stateful decoder
(init, reset, rewind, scan, flush, best end state, get output, deinit), the
correlation decoder `osmo_conv_decode_acc` and the public dispatcher
`osmo_conv_decode`. The harness includes the library's `conv_acc.c` so it can
force each decoder backend.

Snapshots record semantic state and valid history, not C memory layout,
addresses or uninitialized allocation bytes. Inputs and initialized and final
destinations are recorded in full, including suffixes. The harness generates
descriptors and inputs; expected bits, metrics and returns come only from the C
calls.

## Fixtures

`tests/vectors/conv/conv_vectors.ndjson` holds 36,256 flat records: 4 metadata,
1,066 descriptor and 35,186 operation records across 1,707 case identifiers.
Records are labeled by backend:

| Backend | Built from |
| --- | --- |
| `linked-dispatch` | The existing pinned library, through its public dispatcher |
| `scalar`, `sse`, `avx` | Temporary builds of the exact pinned sources, each forced to one backend |

The stateful decoder subset has 332 cases and 7,257 lifecycle observations,
including 664 deinitializations.

`tests/support/conv_fixture.rs` checks schema, provenance, operations, state,
buffers, frozen coverage and backend agreement, and pins FNV-1a fingerprints of
the fixture and both manifests. `tests/conv_reference_vectors.rs` runs it.

## Coverage

Small exhaustive inputs, larger seeded and structured patterns, rates
1, 2, 3, 4, 5, 8 and 9, constraint lengths 2, 3, 5, 7, 9 and 10, termination
modes 0, 1, 2 and 42, recursive codes, puncturing, soft erasures, noise, ties,
the value −128, normalization boundaries, and decoder reuse, reset and rewind.

Two synthetic cases replace protocol vectors that were once taken from upstream
`conv_test.c`: `synthetic-rate2-k7-l97-unpunctured` and
`synthetic-rate2-k7-l97-punctured`, each with six soft modes. For N=2 and K=7
the local rule `(1 << (K-1)) | 1 | (j % 2 ? 2 : 4)` gives generator masks
`0x45` and `0x43`. Both cases draw 97 input bits from a 32-bit LCG seeded with
`0x8f32d6a1` (`seed = 1664525 * seed + 1013904223`, input bit `seed >> 31`);
the hard input's SHA-256 is
`65c593a6bed7222d2c04961409d7953ab0cef862c4df92eb4bebbd9941f878ef`. The
punctured case omits raw positions 0, 3, …, 204 of its 206-symbol stream. No
upstream vector source is read by the current runner.

## Recorded C behavior

- State loading packs and narrows bits instead of walking transitions.
- The output-length helper subtracts every puncture entry, even past the end of
  the stream, so it can differ from the actual write count and can return a
  defined negative length.
- Generic decoding returns a squared-error metric; correlation decoding returns
  status 0 on success. The two are never compared with each other.
- The dispatcher sends K=5 or K=7 with N≤4 to correlation, including N=1,
  which correlation rejects without falling back.
- Reset with `start_state` equal to the state count disables every survivor.
  This is defined: C reset uses an equality loop.
- Scalar, SSE and AVX agree on decoded output and status for every comparable
  frozen case, including the scalar `>=` tie choice. The linked library's
  runtime backend is AVX, identified through pinned ELF symbols. NEON uses a
  strict comparison in the inspected source and was not executed, so it is
  outside this evidence.

## Excluded cases

Short tail-biting input, empty non-flush traceback and unreachable history are
unsafe and are never golden cases. `--audit` builds the isolated scalar, SSE
and AVX paths with ASan and UBSan and requires separate diagnostics for the
invalid shift and signed sizing overflow. The linked library is not
instrumented.

## Provenance

This runner never rebuilds the checkout. `conv_provenance.json` pins 33
participating source, header, template, build and version files plus the
existing library. `conv_generator_provenance.json` pins the harness, the runner,
the source manifest, the record schema and the coverage. Missing or changed
files, unpinned project includes and external library aliases fail the run.

The checkout has no `.git`, so the revision is `unknown`; the embedded version
`1.14.1.1-95043` is descriptive only. The linked-dispatch records are evidence
about the pinned binary, not proof that it was built from the pinned sources.
The isolated backend records are built from the pinned sources.

## Checking

Modes are `all` (default), `linked-dispatch`, `scalar`, `sse`, `avx`,
`--verify` and `--audit`. `all` withholds output if the backends diverge
materially. Keep temporary exact-source builds inside this repository with
`TMPDIR`:

```bash
./reference/conv/run_conv_vector.sh --verify
mkdir -p target/conv
TMPDIR="$PWD/target/conv" ./reference/conv/run_conv_vector.sh --audit
TMPDIR="$PWD/target/conv" ./reference/conv/run_conv_vector.sh all \
  | cmp - tests/vectors/conv/conv_vectors.ndjson
```
