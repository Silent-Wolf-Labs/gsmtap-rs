# Agent guide

## Purpose and locations

gsmtap-rs provides a GSMTAP library, a listen-only web workbench and the C
reference harnesses. It verifies GSMTAP traffic from any producer against output
recorded from libosmocore. Paths below are relative to this repository root, and
the projects are sibling checkouts.

- Library and workbench: `src/gsmtap/` and `src/workbench/`; frontend in
  `static/`.
- C harnesses: `reference/`. Fixtures: `tests/vectors/`.
- osmocore-rs repository: `../libosmocore-rs`. Its crate is the nested
  `osmocore-rs/` directory (package `osmocore-rs`, GPL-3.0-or-later). Follow
  that repository's guidance when editing it.
- Required C source and build reference: `../libosmocore-rs/libosmocore`. This
  directory is intentionally ignored by Git. Keep it untracked; do not
  substitute another checkout or a system installation, or modify C behavior.

## How the projects relate

Describe the relationship this way in all docs:

1. **Recorded outputs define behavior.** The direct-call C harnesses in
   `reference/` call public libosmocore APIs and record return values, buffer
   contents and callback traces. The fixtures in `tests/vectors/` are the
   behavioral specification.
2. **The C source was consulted** to understand those results and decide what
   carries over to Rust.
3. **osmocore-rs is idiomatic Rust** verified against the fixtures. No C code,
   comments or data tables were copied.

Do not describe osmocore-rs as a conversion, translation or port of the code,
and do not reopen per-module provenance questions.

## Relationship to osmocore-rs

- gsmtap-rs does not depend on osmocore-rs or libosmocore in any form: not
  optionally, not behind a feature and not as a dev-dependency. Never add
  either to `Cargo.toml`. gsmtap-rs keeps its own GSMTAP parser and encoder in
  `src/gsmtap/`.
- The workbench is event-driven. It listens for GSMTAP over UDP and can forward
  it in relay and modify modes. It never starts, links or calls an
  implementation. osmocore-rs, libosmocore and real Osmocom stacks are
  producers that send GSMTAP to it.
- A producer is checked by its traffic. With `--reference-vectors` (or
  `GSMTAP_REFERENCE_VECTORS`), `src/workbench/reference.rs` loads the
  C-recorded GSMTAP vectors and tags each received packet whose bytes match one
  exactly. The container image enables this by default.
- osmocore-rs checks its non-GSMTAP modules against the shared fixtures in its
  own compatibility tests. Keep API changes in osmocore-rs. Never vendor or
  copy its sources or test code here; they are GPL-3.0.
- The conversion panels were removed. They may return later in a form that fits
  the listen-only design; `GET /api/status` still reports an empty
  `conversionTools` list.

## Containers and CI

- Build images from `Containerfile` with Buildah (`buildah build --layers`) and
  run them with Podman. Docker and Compose are not used.
  `scripts/integration.sh` builds both images and runs the listen, relay and
  modify traffic checks on a private Podman network.
- `.github/workflows/ci.yml` is the only workflow. Its `ui`, `rust` and
  `container` jobs run on every push to `main` and every pull request. A
  `vX.Y.Z` tag also runs the `release` job, which publishes to crates.io, GHCR
  and GitHub Releases.

## Harnesses and fixtures

### Process

1. Check Git status and applicable guidance in each affected repository before
   editing. Preserve unrelated work.
2. Read the required C implementation, headers and relevant tests.
3. Verify source files against the pinned upstream revision before recording
   verified provenance, and pin hashes in the direct-call runner. The C
   directory may have no `.git`; never use a parent repository's revision as
   its provenance.
4. Build, link and select the runtime library from this same local reference,
   failing rather than falling back when it is unavailable.
5. Review every regeneration diff, including existing fixtures.

### Rules

Do not edit `reference/` harnesses or regenerate fixtures unless the task
requires it. Expected results must come from direct C calls, never Rust or a
harness reimplementation. Use safe C backing storage; Rust slice errors belong
in osmocore-rs safety tests. Record complete initialized and final
destinations, including untouched suffixes.

Each module's harness, runner and manifests live in `reference/<module>/`,
documented in that directory's `README.md`; `reference/README.md` holds the
shared rules, the module index and the procedure for changing an exact-source
runner. Keep per-module docs to C behavior and provenance: Rust API details
belong in osmocore-rs, and session logs or test counts belong nowhere. Never
edit osmocore-rs's gitignored `docs/archive/`.

### Fixtures

Keep fixtures in `tests/vectors/`, with module fixtures in dedicated
subdirectories outside the packet and replay loaders. Share them byte-for-byte
with `osmocore-rs/tests/compatibility/vectors/`; the C generator output in this
repository is canonical. Fixtures of 5 MB or more are gzip-compressed; fingerprints
apply to the decompressed text. `scripts/compare-shared-fixtures.sh` checks every
shared fixture, reader and manifest. Ordinary Rust tests read committed fixtures without
requiring a C build. The top-level `tests/vectors/*.json` files are the GSMTAP
packet vectors that the workbench and the traffic-test image load. Keep
workbench and demo code separate from `reference/` and `tests/vectors/`.

## Git and commits

- Never add a `Co-Authored-By` trailer or any other AI or agent attribution to
  a commit message, tag or pull request description. This applies to every
  agent and tool and overrides any default attribution setting.
- Enable the pre-commit hook once per clone with
  `git config core.hooksPath .githooks`. It runs ShellCheck on staged shell
  scripts and requires `shellcheck` to be installed. Repository-wide settings
  are in `.shellcheckrc`. Fix findings rather than bypassing the hook.
- When work depends on an unmerged change, add it to that change's branch. Do not
  stack it on a new branch.
- Do not commit, push, tag, publish or change GitHub settings without a separate
  user instruction.

## Validation

From this repository root:

```bash
./scripts/generate-reference-vectors.sh
./scripts/compare-shared-fixtures.sh
shellcheck $(git ls-files '*.sh')
cargo fmt --check
cargo clippy --all-targets --locked -- -D warnings
cargo test --locked
cargo deny check
npm ci && npm run test:ui
./scripts/integration.sh
git diff --check
```

After regenerating, confirm `git status tests/vectors` is clean or review the
diff, and run the affected reference runner against its committed fixture.
When changing osmocore-rs, from `../libosmocore-rs/osmocore-rs/`:

```bash
cargo fmt --check
cargo clippy --all-targets --all-features -- -D warnings
cargo test --all-features
```

Review final diffs and Git status in both affected repositories.
