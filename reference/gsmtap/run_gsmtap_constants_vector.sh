#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
header_hash=$(sha256sum "$reference_dir/include/osmocom/core/gsmtap.h" | cut -d ' ' -f1)
[ "$header_hash" = 5f14e5002b8d1d9d37beacea0d4c7786866e14cd3731d641ad3e98399214ee19 ] || {
    echo 'C reference hash mismatch: include/osmocom/core/gsmtap.h' >&2
    exit 1
}
binary=$(mktemp "${TMPDIR:-/tmp}/gsmtap-constants-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

# The constants are macros, so the harness needs only the header.
cc -std=c11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gsmtap_constants_vector.c" \
	-o "$binary"

"$binary"
