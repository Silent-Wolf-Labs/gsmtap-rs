#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm0502.c 76abe3a746dea79336f033bde415d057b973a324dbc2f4e1d81cb49a738c3d15
check_hash include/osmocom/gsm/gsm0502.h 8d4a3d87207a5a00c82cde350a97183b2618788b5e01ed7f922ce83a2a2ba3ee
check_hash src/gsm/gsm48.c 9c9bd8925a06cc5d3fc607e5667cc9056793280f190c76e3af810d568a0477c1
check_hash src/gsm/rsl.c 0e41022916373b5e5879065c7a677d13265845e4355a4837d2c8d5077e9a1f8b
check_hash src/gsm/gsm_utils.c 6f4479f8d9a49bec5c5ed76ca7976c1c11f741e3b1519f3c3aa18d9b42cda743
check_hash tests/gsm0502/gsm0502_test.c 419adea769ec33e0635980921302b40a130876b021727b1de634a48cb6b693f2
check_hash tests/gsm0502/gsm0502_test.ok 2a13570999228e39aab549001ccfef38080dcab794a30c656e1cf7d69fc2d0f0
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/gsm0502-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm0502_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm0502/gsm0502_test.ok"
