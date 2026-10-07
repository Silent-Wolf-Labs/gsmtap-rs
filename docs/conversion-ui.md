# Conversion UI: Base64 review stage

The workbench now has a view selector for Packets, Hexparse, BCD, Bits, and
Base64. Conversion panels are available in every operating mode. Each panel
retains its inputs and results when another view is selected. Packet polling
continues independently and does not rebuild conversion forms.

Hexparse is now executable through `POST /api/conversions/hexparse`. Its UI
shows parsed hex, UTF-8 text when valid, errors, and a paged comparison of the
complete initial and final buffers. Copy controls provide parsed hex, text,
and the complete final buffer (including on failure). Invalid input, odd
nibbles, and capacity failures preserve the Rust parser's partial writes.

BCD is now executable through `POST /api/conversions/bcd`. It supports
character/nibble helpers and string/buffer conversions, automatic or explicit
nibble ranges, and hexadecimal-digit allowance. Results distinguish the
reported digit count from the actual truncated text, and retain full buffers
and partial writes on errors. Scalar helpers display their result without a
buffer table.

Bits is executable through `POST /api/conversions/bits`, with basic and
extended pack/unpack operations. Source format can be text, hex bytes, or
unpacked `0/1` digits for packing. Results show complete destination bytes,
binary representation for packing, written bit values for unpacking, and a
paged before/after comparison. Base64 is executable through `POST /api/conversions/base64`, supporting
encoding, decoding, and decode size probes. Local, CI, and Docker builds use
`libosmocore-rs` version `0.1.0` from an exact snapshot in `vendor/libosmocore-rs`.
The source repository is private, so using a Git URL would require credentials
in both Cargo and Docker. The snapshot records revision
`25c4bae36a35ca2562e56db384370363ec2a6603` and every source file's SHA-256 hash.
The sibling project remains the source of API implementations; update the
snapshot from a reviewed committed revision rather than editing it here.
Crates.io publication still requires a published conversion dependency.

## Frontend structure

- `static/models/conversion/`: four input models, a shared destination-buffer
  model, common field validation, and a shared output model.
- `static/controllers/conversion-controller.js`: independent tool states,
  validation, submission, reset, duplicate-request prevention, cancellation,
  and rejection of stale responses.
- `static/controllers/navigation-controller.js`: view selection without
  rebuilding panels or changing the workbench operating mode.
- `static/services/conversion-service.js`: JSON requests, HTTP errors, and
  cancellation. Availability is per tool; all four tools are enabled by default.
- `static/components/conversion/conversion-panel.js`: labeled forms and local feedback.
  `hexparse-result.js` assembles the reusable buffer table and copy controls;
  `conversion-field.js` supplies shared text, hex, numeric, and option inputs.

## Endpoint contract

The service targets `POST /api/conversions/{hexparse,bcd,bits,base64}`.
All four endpoints are implemented. A Hexparse request has this shape:

```json
{
  "operation": "parse",
  "source": { "encoding": "text", "data": "CA FE" },
  "destination": { "capacity": 4, "fillByte": 170, "initialHex": "" },
  "options": {}
}
```

`text` represents UTF-8 source bytes; `hex` represents exact source bytes.
Hexparse text is the actual parser input, so malformed hex is submitted to
Rust rather than rejected by the UI. Hex representation must contain complete
bytes. Neither format implies a trailing NUL; source bytes can include NUL
explicitly. BCD and hexparse use their Rust APIs' first-NUL/slice-end behavior.

Destination capacity is limited to 65,536 bytes. `fillByte` is a decimal byte
used to initialize the entire destination unless `initialHex` is supplied.
Explicit initial bytes must match the capacity exactly. Source text is limited
to 196,608 characters; the backend also bounds source bytes at 196,608 and request bodies at 2 MiB
independently of client validation.

Operations and options:

| Tool | Operations | Options |
| --- | --- | --- |
| Hexparse | `parse` | None |
| BCD | `str2bcd`, `bcd2str`, `char2bcd`, `bcd2char` | Buffer operations: `startNibble`, `endNibble`, `allowHex`; automatic encoding end is `null` |
| Bits | `pack`, `unpack`, `pack-ext`, `unpack-ext` | `numBits`; extended operations: `inputOffset`, `outputOffset`, `lsbMode` |
| Base64 | `encode`, `decode` | `initialOutputLength`; decode also has `sizeProbe` |

BCD scalar operations consume exactly one source byte. `bcd2char` accepts
values 0–15. For these operations, and Base64 decode size probes, destination
is `null`. Bit pack input consists of byte-sized bit values, rather than a
string of binary digits. Offsets use the Rust functions' units: extended pack
uses source bytes and destination bits; extended unpack uses source bits and
destination bytes. The UI does not reinterpret C-compatible nonbinary values.

A result includes:

```json
{
  "operation": "parse",
  "success": true,
  "returnValue": 2,
  "outputLength": null,
  "outputHex": "CA FE",
  "outputText": null,
  "initialDestinationHex": "AA AA AA AA",
  "finalDestinationHex": "CA FE 00 00",
  "error": null
}
```

`returnValue` follows the selected Rust API; errors may use `null` for APIs
returning `Result`. `outputLength` is the final caller-owned Base64 length
value when applicable, including on failure. Returned positions, lengths,
and actual writes must remain distinct. In particular, do not slice a
destination using an unchecked return value. Complete initialized and final
buffers must be returned on conversion errors as well as successes.

Conversion failures are structured results (`success: false`) in a successful
HTTP response. Invalid request structure, oversized requests, and transport
failures use HTTP errors. Base64 returns its signed C-compatible status in `returnValue`.

## Review

Run the workbench and use **Workbench view** to inspect each form. Enter
values, switch views, and return to check preservation. Use Reset to restore
only the active tool's defaults. On Hexparse, enter `CA FE`, capacity `4`,
and fill byte `170`, then Convert: the table should show `AA AA AA AA`
becoming `CA FE 00 00`. Try `12 A?` to inspect partial writes on failure.
Copy final buffer remains available after a conversion failure. Larger
buffers use 128-row pages while retaining every byte. Base64 is enabled for this review stage.

## BCD review examples

- Encode digit string `123`, start nibble `0`, automatic end, capacity `4`,
  fill byte `170`: final destination is `21 F3 AA AA`.
- Decode hex source `21 FA`, nibble range `[0, 4)`, allow hexadecimal digits,
  capacity `3`: text is `12`, reported count is `4`, and truncation is shown.
- Encode text `1x`, start nibble `1`, explicit end `4`, capacity `3`, fill
  byte `170`: the invalid-digit result preserves final buffer `1A AA AA`.
- Character-to-nibble with text `A` returns `10` (`0A`). Nibble-to-character
  with hex source `0F` returns `F` (character byte `46`).

For buffer decoding, `outputText` and `outputHex` contain only the text actually
placed before the NUL terminator, including disallowed digits on an
invalid-digit result. For encoding, `outputHex` is the complete destination,
including untouched bytes: the returned byte position can understate writes
for explicit odd nibble endpoints. `returnValue` is null on conversion errors,
and `error.code` distinguishes `invalidDigit`, `noSpace`, and `outOfRange`.
Scalar operations require a single source byte, no destination, and empty
options. Buffer operations require a destination and `startNibble`; decoding
also requires `endNibble`. Offsets are bounded at 131072 nibbles.

## Bits review

Choose Pack, source format **Unpacked bits (0/1)**, and enter a sequence such
as `0 1 1 0`. Set Number of bits to `4`, then Convert. For raw byte values
outside 0 and 1, use hex format: basic packing shifts each entire source byte;
extended packing treats nonzero bytes as one.

Extended pack uses input offsets in unpacked bytes and output offsets in
packed bits. Extended unpack uses input offsets in packed bits and output
offsets in unpacked bytes. Check **Use LSB-first ordering** to reverse the
within-byte ordering for extended operations. Bits outside the selected
range remain unchanged, including neighboring bits within partial bytes.

All counts and offsets are bounded at 524288, and destination buffers at
65536 bytes. Basic operations reject offset and ordering options. The `bits`
source format is available only for packing, accepts `0` and `1` with ASCII
spaces/tabs/newlines, and maps each digit to a source byte; it does not pack
bits in the browser. Unpacking requires packed source bytes.

`outputHex` includes the complete destination. For unpacking, `outputText`
contains only the bit values written, excluding preserved prefix/suffix bytes.
The return value is a byte count for basic operations and an ending byte
position for extended operations; it is never used to size the displayed
buffer. Zero-bit extended packing at output offset zero retains the C API's
compatibility return value `536870912` while writing nothing. Zero-bit basic
unpacking writes one bit when source and destination backing storage exist;
extended unpacking writes nothing. Source/destination capacity failures are
structured results, with `inputTooShort` or `outputTooShort` error codes and
unchanged destination bytes.

## Base64 review

- Encode text `foo`, capacity `7`, fill byte `170`: text `Zm9v`, output length
  `4`, full buffer `5A 6D 39 76 00 AA AA`. The NUL needs an extra byte.
- Encode with capacity `4`: status `-105`, required capacity `5`, unchanged
  buffer. Encoding with capacity zero also probes required capacity.
- Decode text `TWE=`, capacity `4`, fill byte `170`: text `Ma`, output length
  `2`, full buffer `4D 61 AA AA`.
- Decode `TWE=` with **Probe decoded size without a destination**: status
  `-105`, required capacity `2`, no destination allocation or buffer table.
- Decode `TW$=`: status `-22`, unchanged buffer and initial output-length
  value. Decode empty or whitespace-only input: status `0`, unchanged
  length value and buffer, empty converted output.

`initialOutputLength` accepts unsigned values up to 4294967295. `outputLength`
is always the final caller-owned variable, not necessarily bytes written.
`sizeProbe` identifies decode probes; `success` reflects the raw return code,
so a successful nonempty size measurement retains `success: false` and
`bufferTooSmall`. The UI presents that expected probe result as required capacity.
`outputHex`/`outputText` contain only successfully converted bytes (excluding
encoding's NUL); invalid UTF-8 decoded bytes have no text representation.
Complete initial/final buffers remain inspectable and copyable on errors.
Unknown operations/options, missing destinations, and encoding probes with
`sizeProbe: true` are rejected as request errors. The backend calls the existing
Rust API without changing its whitespace or unpadded-input behavior.
