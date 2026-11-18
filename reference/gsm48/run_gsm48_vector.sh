#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm48.c 9c9bd8925a06cc5d3fc607e5667cc9056793280f190c76e3af810d568a0477c1
check_hash src/gsm/gsm23003.c efdfebf096071e8b24fa51c4acce6cdefc4b71824ccfb993fd703955e70e6f16
check_hash include/osmocom/gsm/gsm48.h 578039a3cdfdfecafc4c2ef753d4ab678e05d30ae79d3aa95116919256e74cc2
check_hash include/osmocom/gsm/gsm23003.h 1c04d7cdf3a568ba72de26007efd1c8fa2e48fd057b29f5a9df8b794b4c4aa3c
check_hash include/osmocom/gsm/protocol/gsm_04_08.h 33968a5ad3b54a5e56a8159f489380cac41d6ce7bc00e2ed9b6df9b5db7f67e7
check_hash include/osmocom/gsm/protocol/gsm_23_003.h de16b434373d4b347e1fa186ce1943fc3dcdbeeaac7f0b9335f5da166832fb35
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
binary=$(mktemp "${TMPDIR:-/tmp}/gsm48-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

# The legacy functions under test are declared deprecated.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm48_vector.c" \
	"$gsm" "$core" -ltalloc \
	-Wl,-rpath,"$libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm0408/gsm0408_test.ok"
