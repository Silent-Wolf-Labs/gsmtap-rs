#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/core/use_count.c 98ec2d495faa28d6361ac9ac6f634e35fa140a12e94672ddec1e6af4cb1eeb6e
check_hash include/osmocom/core/use_count.h 78b27494d0bdb7fb0e2d071a7bb186b83178a86a6afe770a3efa05999550def2
check_hash tests/use_count/use_count_test.c bc52f25a1ac9e35676928522f50c59def8a4c60f15053e9b21ac30aab8e9fd29
check_hash tests/use_count/use_count_test.err 14a61c355a27d7a5c8f498a8ce05ea2f883b2dbac1ebb158a0336098173f2c41
make -C "$reference_dir/src/core" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
[ -f "$core" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/use-count-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/use_count_vector.c" \
	"$core" -ltalloc \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/use_count/use_count_test.err"
