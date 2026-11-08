# sockaddr_str reference

`sockaddr_str_vector.c` calls the public `sockaddr_str.h` API directly and records the
family and errno constants, then one section per kind of record:

- `type` for `osmo_ip_str_type()` on each text, and on NULL;
- `case` for a stored address: its `OSMO_SOCKADDR_STR_FMT` text, `osmo_sockaddr_str_is_set()`,
  `osmo_sockaddr_str_is_nonzero()`, and the result and output of each `osmo_sockaddr_str_to_*()`;
- `from` for each `osmo_sockaddr_str_from_*()`, with its input and the destination after
  the call. The destination holds junk beforehand, so writes made before a failure show;
- `cmp` for the sign of `osmo_sockaddr_str_cmp()`, since its size comes from libc.

The texts cover valid, nearly valid and invalid addresses of both families, each stored
with `AF_INET`, `AF_INET6`, `AF_UNSPEC` and an unknown family, and text around the 46-byte
field. Conversions from addresses cover every pattern of zero groups in IPv6, IPv4 tails,
and socket addresses of each family. Comparisons add pairs that go round in a cycle,
unknown families and text that doesn't parse.

Results depend on the host as well as on libosmocore: the address text comes from libc's
`inet_pton` and `inet_ntop`, and IPv4 comparison and the 32-bit values depend on byte order.
The fixture was recorded with glibc 2.39 on a little-endian host. A `sockaddr_storage` is
recorded only up to the size of a `sockaddr_in6`; the harness checks the rest stays zero.

## Fixtures

`tests/vectors/sockaddr_str/sockaddr_str_vectors.ndjson` starts with a metadata line, followed
by one section per kind of record. `tests/support/sockaddr_str_fixture.rs` pins an FNV-1a
fingerprint of the file and checks the sections, fields, provenance and value ranges.

## Provenance

The runner checks the hashes of `src/core/sockaddr_str.c`,
`include/osmocom/core/sockaddr_str.h`, `tests/sockaddr_str/sockaddr_str_test.c` and
`tests/sockaddr_str/sockaddr_str_test.ok`, verified byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

The harness replays upstream's test with the same calls, records them with the source
`upstream`, and fails unless the output it prints matches `sockaddr_str_test.ok` byte for
byte.

## Checking

```bash
./reference/sockaddr_str/run_sockaddr_str_vector.sh | cmp - tests/vectors/sockaddr_str/sockaddr_str_vectors.ndjson
```
