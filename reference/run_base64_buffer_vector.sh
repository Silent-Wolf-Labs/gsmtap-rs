#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/core/base64.c fe3d63a9ba4579d669faf35c47edd93cdccf1ed139009aab219de2f54b2ca37a
check_hash include/osmocom/core/base64.h 602288368adc1e55e24836472dd839f3650fd6946dd67ac78093fe90ab128d99
check_hash tests/base64/base64_test.c 74b82ceb47871740f217d519c19662546bb900f17e06109992fd6fa41d85d5b3
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
[ -f "$library" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/base64-buffer-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror \
	-I"$reference_dir/include" \
	"$script_dir/base64_buffer_vector.c" \
	"$library" \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

if [ "$#" -gt 0 ]; then
    "$binary" "$@"
else
    printf '[\n'
    separator=''
    for case_name in $("$binary" --list); do
        printf '%s' "$separator"
        "$binary" --case "$case_name" | tr -d '\n'
        separator=',
'
    done
    printf '\n]\n'
fi
