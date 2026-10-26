# GSMTAP encoder reference

`gsmtap_makemsg_ex_vector.c` selects a named input from a small table and calls
`gsmtap_makemsg_ex()` or `gsmtap_makemsg()`. It writes one JSON observation
with the return code, whether a message was created, its length and the
complete message as lowercase hexadecimal.

## Fixtures

The seven cases are written to `tests/vectors/<case>.json`, one file per case.
These are the GSMTAP packet vectors: the gsmtap-rs decoder tests, the
workbench's `--reference-vectors` matching and the traffic-test image all load
them. Other modules' fixtures live in subdirectories so those loaders never see
them.

| Case | Covers |
| --- | --- |
| `gsmtap_v2_basic_header` | Um header with ordinary field values and a 4-byte payload |
| `gsmtap_um_uplink_only` | Um with the uplink ARFCN flag (`0x4001`) |
| `gsmtap_um_pcs_empty_payload` | Um with the PCS ARFCN flag (`0x8002`) and an empty payload |
| `gsmtap_um_uplink_pcs_boundary` | Um with ARFCN `0xffff`, the largest frame number, signal −128 and SNR 127 |
| `gsmtap_abis_signed_boundaries` | Abis with ARFCN `0x7fff`, signal 127 and SNR −128 |
| `gsmtap_sim_atr` | SIM type with an ATR payload |
| `gsmtap_makemsg_um_wrapper` | The `gsmtap_makemsg()` Um wrapper |

The `case` field is a stable identifier. Renaming, removing or materially
changing a case is a fixture-contract change.

## Provenance

The fixtures record `libosmocore_commit`
`950430e829a3dc1d162aa241bc0505745c5a7311`. This runner pins no source hashes.
It rebuilds the checkout's `src/core` and links against it.

The committed files use indented JSON. Regeneration writes the same layout, so
a full regeneration is byte-identical.

## Checking

```bash
./reference/gsmtap/run_gsmtap_makemsg_ex_vector.sh --list
./reference/gsmtap/run_gsmtap_makemsg_ex_vector.sh --case gsmtap_sim_atr \
  | cmp - tests/vectors/gsmtap_sim_atr.json
```

Changed encoded bytes need an explanation before the regenerated fixture is
accepted. A changed revision with identical bytes is informational.
