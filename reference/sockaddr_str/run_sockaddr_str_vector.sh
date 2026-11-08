#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/core/sockaddr_str.c 0693cae63d6e2b533e2d1f9430963df9247753812824cd45dadf6d67b11d3f33
check_hash include/osmocom/core/sockaddr_str.h 5517b1292bd63f865664ad982c54b4346a2d2d2c7597832b51b464e714941120
check_hash tests/sockaddr_str/sockaddr_str_test.c 44cd00685507e31626f4d1de420e59ff58e6abb9a095c3fd3d4a21e7f9d68a45
check_hash tests/sockaddr_str/sockaddr_str_test.ok c6fab9cb758e34634b39391f7c765ae86ccf38520e4954f78a77a365b1a8b48c
make -C "$reference_dir/src/core" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
[ -f "$core" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/sockaddr-str-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/sockaddr_str_vector.c" \
	"$core" \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/sockaddr_str/sockaddr_str_test.ok"
