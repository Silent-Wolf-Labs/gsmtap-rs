#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/core/strrb.c 1d923a008902c294ea8efb4690a64b203698ac78a4016ac9c338a5dd519dbab0
check_hash include/osmocom/core/strrb.h 8d58b8fd9d0c76da3f50e9ba62fcf600323153250f4f57a98c5e11089e72185d
check_hash tests/strrb/strrb_test.c 95c88ffffe211a7034642a7add943c53f8e62467d8b132c4524845bbc634a5b2
check_hash tests/strrb/strrb_test.ok 754ca4482055366f097c1dcfbbf228fbbf398ff7776c4867487a4c53944eb55b
make -C "$reference_dir/src/core" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
[ -f "$core" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/strrb-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/strrb_vector.c" \
	"$core" -ltalloc \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/strrb/strrb_test.ok"
