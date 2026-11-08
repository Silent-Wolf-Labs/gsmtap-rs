#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/comp128.c d2b7267c7a1ec95bd0ac43b5d1ab415322593e3d5d589e6446896595ffc533fc
check_hash src/gsm/comp128v23.c 3864190e9957e3a13ea066798034faf3fea040718d6a515600a30a5591d71e31
check_hash src/gsm/auth_comp128v1.c 7a5cd3e98708dee58d33065be015f31b911aa08e61cbd4fd61719bc10e0cf502
check_hash src/gsm/auth_comp128v23.c d10904dc60e8643757c44b295498c0b2624387199fe3161cd01402508a0bb1c8
check_hash src/gsm/auth_xor_2g.c b68f47dec119d34a226a9f32119ccfdfb6f432b99ae92be88bfde72b6d339c6b
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash include/osmocom/gsm/comp128.h 694438188a8b96719f873807c68233dcb2b2df2ae2f170d270acd336364d3db5
check_hash include/osmocom/gsm/comp128v23.h 775b6e2f3d08a7efcbadb1095216aa58bbc3e6e9be955f42721ea78e6eee1f70
check_hash include/osmocom/crypt/auth.h 97e10cca85377d6960921dbfe798db2c69a2a4d288bc69e7ee32af517b385162
check_hash tests/comp128/comp128_test.c b033bb13d13438f723bab70f339a178e50342f498d9f96fa5f75e17d6c394d22
check_hash tests/comp128/comp128_test.ok f6d7c96750168e550e7e87471aa6e37acf88c7504f742ebecbbeef312b662929
check_hash tests/auth/xor2g_test.c 728eaf1c220799ae1cce1e90d3f9c8b040d898c5e0a5d0f2a118fd7ab7f29645
check_hash tests/auth/xor2g_test.ok 1488c564fff5b7bede4a1e04926b473af764337341bf9252a7a3d5faff20aa36
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/comp128-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/comp128_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/comp128/comp128_test.c" \
	"$reference_dir/tests/comp128/comp128_test.ok" "$reference_dir/tests/auth/xor2g_test.ok"
