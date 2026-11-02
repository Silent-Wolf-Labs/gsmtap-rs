#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/core/prbs.c a9cb5d26dd3babf6340f81f8dbea27167f591f0a6db00ae08d600531ee7b15d8
check_hash include/osmocom/core/prbs.h 0082cc6950871bebec1c3f71c10fe3bc4e00117381b892e0ab095c90529ec9d1
check_hash tests/prbs/prbs_test.c a99cf82d86d22744b04ddd5dc249df81713d4bfdb22a9599f19611f4495622dc
check_hash tests/prbs/prbs_test.ok ad2050b4a4862d9d4b2df610a88b8178685717268f5bf0a7798e424e41282cca
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
[ -f "$library" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/prbs-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror \
	-I"$reference_dir/include" \
	"$script_dir/prbs_vector.c" \
	"$library" \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary"
