# TLV reference

`tlv_vector.c` calls the public `libosmogsm` API directly and records:

- `tlv_parse_one()` for each format at its length boundaries, including the outputs
  it has written by a failure;
- `tlv_parse2()` with repeated tags, leading LV tags, truncated input and random input;
- `tlv_encode_one()`, `tlv_encode()` and `tlv_encode_ordered()`;
- each `msgb_*` TLV helper, and the gross length helpers;
- `osmo_shift_v_fixed()`, `osmo_match_shift_tv_fixed()`, `osmo_shift_tlv()`,
  `osmo_match_shift_tlv()` and `osmo_shift_lv()`;
- `osmo_tlvp_merge()`, `osmo_tlvp_copy()`, `tlv_def_patch()` and the accessors;
- `osmo_tlv_prot_parse()`, `osmo_tlv_prot_validate_tp()` and the naming functions.

Value bytes follow a fixed pattern, `(index * 37 + 0x5b) & 0xff`. Output over 256
bytes is recorded as its length and FNV-1a hash.

## C behavior worth knowing

- `osmo_tlvp_merge()` copies each value but never sets its length, so a merged tag has
  length zero.
- `osmo_tlvp_copy()` only allocates a value if the new, empty entry already has one, so
  it copies no values. Every tag of the copy is absent, with its length set.
- `tlv_parse2()` reads the byte after an exhausted buffer for an LV tag. The harness
  keeps that byte in bounds, and the status doesn't depend on its value.
- When both LV tags name the same tag, the second replaces the first in `dec[0]`
  instead of going in `dec[1]`, or is dropped when there's one entry.
- Tvlv and GAN lengths from `0x8000`, and GAN tags from `0x8000`, set a high bit that
  the parser reads as the other form.
- `msgb_tvl_put()` reserves the value, though its comment says it doesn't.
- `tlv_encode_one()` truncates a length to the format's field.

Calls that would write or read out of bounds aren't recorded: `msgb_put()` with no
room, Tvlv lengths whose `uint16_t` gross length wraps, `tlvp_val16_unal()` on a short
value, and `tlv_encode_one()` of `TLV_TYPE_TV` with no value.

## Fixtures

`tests/vectors/tlv/tlv_vectors.ndjson` starts with a metadata line, followed by one
record per call. `tests/support/tlv_fixture.rs` pins an FNV-1a fingerprint of the file
and checks the sections, fields, provenance and internal consistency, such as a value
offset staying inside its element.

## Provenance

The runner checks the hashes of the parser, message buffer and logging sources and
headers, and of `tests/tlv/tlv_test.c` and `tests/tlv/tlv_test.ok`. All are verified
byte-for-byte against upstream revision `950430e829a3dc1d162aa241bc0505745c5a7311`.

The runner also builds upstream's `tlv_test.c` and fails if its output differs from
`tlv_test.ok`.

## Checking

```bash
./reference/tlv/run_tlv_vector.sh | cmp - tests/vectors/tlv/tlv_vectors.ndjson
```
