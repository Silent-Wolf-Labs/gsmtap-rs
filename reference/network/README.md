# Networking establishment reference

This focused harness calls `osmo_sock_init2()` and `osmo_sock_set_dscp()` in the
required local C checkout. It records six stable IPv4 loopback cases: ordinary
and configured UDP bind, invalid DSCP rejection, connected UDP payload transfer,
TCP listen and TCP local-bind/connect payload transfer. Ephemeral port numbers
are normalized to bound/peer-match predicates; they are not frozen as constants.
No public network services, DNS servers or privileged ports are used. IPv6,
connect timeouts and Rust validation/cleanup have independent Rust tests.

`provenance.json` pins socket source/header/test files verified byte-for-byte
against revision `950430e829a3dc1d162aa241bc0505745c5a7311` on 2026-10-16.
`generator_provenance.json` pins this harness, runner and provenance record.
The runner fails closed on mismatches, builds/links/selects only the required
local library and writes normalized TSV to stdout. It does not generate or
replace existing fixtures. This is bounded capability evidence, not a claim of
C socket API/errno/address-order parity.

From the gsmtap-rs root:

```sh
./reference/network/run.sh | cmp - tests/vectors/net/network_vectors.tsv
cargo test --test network_reference_vectors
```

For intentional regeneration, review fresh stdout before replacing either copy
of `network_vectors.tsv` in `tests/vectors/net/` and the sibling crate's
`tests/compatibility/vectors/net/`. Keep the source provenance record and the
independent `tests/support/network_fixture.rs` identical between repositories.
Normal Rust tests consume frozen observations without building C. The fixture
and reader are separate from all packet/replay/workbench APIs. Existing vendor
snapshots, fixtures, APIs and unrelated pending changes are untouched.
