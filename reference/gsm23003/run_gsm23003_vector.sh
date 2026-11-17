#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm23003.c efdfebf096071e8b24fa51c4acce6cdefc4b71824ccfb993fd703955e70e6f16
check_hash include/osmocom/gsm/gsm23003.h 1c04d7cdf3a568ba72de26007efd1c8fa2e48fd057b29f5a9df8b794b4c4aa3c
check_hash include/osmocom/gsm/protocol/gsm_23_003.h de16b434373d4b347e1fa186ce1943fc3dcdbeeaac7f0b9335f5da166832fb35
check_hash src/core/utils.c a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
check_hash include/osmocom/core/utils.h 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d
check_hash tests/gsm23003/gsm23003_test.c 26979dc26a5fd691b3b31e9648bdcb4d5b9d94933a623fb6d0390ef8be0e54bf
check_hash tests/gsm23003/gsm23003_test.ok 3a2283ea13d077f410c07c53f84f478a0391b978d0d980eb91347f460359f6e6
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/isdn" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
isdn="$reference_dir/src/isdn/.libs/libosmoisdn.so"
# The internal archive the shared libosmogsm is built from, as upstream's tests link.
# libosmogsm.so doesn't export osmo_sai_name and its variants.
gsmint="$reference_dir/src/gsm/.libs/libgsmint.a"
# shellcheck disable=SC2015  # The block is meant to run if any test fails.
[ -f "$core" ] && [ -f "$isdn" ] && [ -f "$gsmint" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/gsm23003-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm23003_vector.c" \
	"$gsmint" "$isdn" "$core" -ltalloc \
	-Wl,-rpath,"$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm23003/gsm23003_test.ok"
