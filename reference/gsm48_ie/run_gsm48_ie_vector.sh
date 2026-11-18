#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm48_ie.c 6d15ada26d2f6e4d4c3dad896188adfc11a2ccf29683ae492054ec2d4e25bbae
check_hash include/osmocom/gsm/gsm48_ie.h b8f4ce22638c61c2de7232fa43fe63c404fbebd694092002b536bbb11c172b61
check_hash src/core/utils.c a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
check_hash include/osmocom/core/utils.h 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d
check_hash tests/gsm0408/gsm0408_test.c 018bd45a186112a112eb0e77fa1b25139fa538f3a652df96b4c20984d0c75bb8
check_hash tests/gsm0408/gsm0408_test.ok d0c420277013debe01bd4ee06e7762394caacd4db7739d614cfdb8f0f07fcc40
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/isdn" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
libs="$reference_dir/src/gsm/.libs:$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs"
export LD_LIBRARY_PATH="$libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/gsm48-ie-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

# gsm48_decode_bcd_number() is declared deprecated.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm48_ie_vector.c" \
	"$gsm" "$core" -ltalloc \
	-Wl,-rpath,"$libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm0408/gsm0408_test.ok"
