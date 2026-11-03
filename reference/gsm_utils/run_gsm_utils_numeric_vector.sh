#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm_utils.c 6f4479f8d9a49bec5c5ed76ca7976c1c11f741e3b1519f3c3aa18d9b42cda743
check_hash include/osmocom/gsm/gsm_utils.h 9435e57b0af45b8b7ec94a3330b6f257e0fdb411a5f36bb0113845046c727064
check_hash include/osmocom/gsm/gsm0502.h 8d4a3d87207a5a00c82cde350a97183b2618788b5e01ed7f922ce83a2a2ba3ee
check_hash include/osmocom/core/utils.h 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d
check_hash tests/gsm0408/gsm0408_test.c 018bd45a186112a112eb0e77fa1b25139fa538f3a652df96b4c20984d0c75bb8
check_hash tests/gsm0408/gsm0408_test.ok d0c420277013debe01bd4ee06e7762394caacd4db7739d614cfdb8f0f07fcc40
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/gsm-utils-numeric-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm_utils_numeric_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm0408/gsm0408_test.ok"
