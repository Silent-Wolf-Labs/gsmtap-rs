# Base64 reference

`base64_buffer_vector.c` calls `osmo_base64_encode()` and
`osmo_base64_decode()` directly. Every destination byte is initialized to
`0xa5` and the output length to `0xdecafbad` before the call. Each observation
records the operation, source bytes, destination presence and length, return
code, output length and the complete final destination, including untouched
suffix bytes.

## Fixtures

`tests/vectors/base64/base64_buffer_vectors.json` is a JSON array of 24
observations. gsmtap-rs has no reader for this fixture; osmocore-rs validates
it in its own compatibility tests.

## Provenance

The runner checks the hashes of `src/core/base64.c`,
`include/osmocom/core/base64.h` and `tests/base64/base64_test.c`, verified
byte-for-byte against upstream revision
`950430e829a3dc1d162aa241bc0505745c5a7311`.

## Checking

The runner writes the full array with no arguments, lists case names with
`--list` and writes one case with `--case NAME`.

```bash
./reference/base64/run_base64_buffer_vector.sh | cmp - tests/vectors/base64/base64_buffer_vectors.json
```
