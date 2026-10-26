# Networking reference

`network_vector.c` calls `osmo_sock_init2()` and `osmo_sock_set_dscp()` on IPv4
loopback. It uses no public network services, DNS servers or privileged ports.

## Fixtures

`tests/vectors/net/network_vectors.tsv` has a header comment and six cases:

| Case | Covers |
| --- | --- |
| `udp_bound` | Ordinary UDP bind |
| `udp_configured` | UDP bind with nonblocking, address reuse and DSCP 46 |
| `invalid_dscp` | Rejected DSCP value |
| `udp_connected` | Connected UDP with a payload round trip |
| `tcp_listener` | TCP listen |
| `tcp_connected` | TCP local bind and connect with a payload round trip |

The columns are case, bound, connected, nonblocking, address reuse, DSCP and
payload match. Ephemeral port numbers are reduced to bound and peer-match
flags rather than recorded. `tests/support/network_fixture.rs` reads the
fixture. This is bounded evidence of socket setup, not a claim of parity with
C socket error codes or address ordering.

IPv6, connect timeouts and cleanup are tested only in osmocore-rs.

## Provenance

`provenance.json` pins the socket source, header and test files, verified
byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`. `generator_provenance.json` pins
this harness, the runner and `provenance.json`. Both are copied to
`tests/vectors/net/`. The runner rebuilds `src/core` and links against it.

## Checking

The full generator does not run this harness.

```bash
./reference/network/run.sh | cmp - tests/vectors/net/network_vectors.tsv
```
