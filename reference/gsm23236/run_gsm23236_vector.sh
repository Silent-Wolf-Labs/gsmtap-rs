#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gsm23236.c 1c62c9b0b755a3a2b6545a86c827cd42434f0613d9fdadb78aaf1c85bbc61e76
check_hash include/osmocom/gsm/gsm23236.h 481a21941c6403d6f7fe3a32c72f5a23ee75ad7fa9bdccd3478258875b822d0a
check_hash src/core/utils.c a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
check_hash include/osmocom/core/utils.h 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d
check_hash include/osmocom/core/linuxlist.h 9015f71c9e30755d48f8f8597c8a5362cb98f5acccf5149b9cf6a5a6b508e39f
check_hash tests/gsm23236/gsm23236_test.c 134aae34da66ba6716f074d8849ee334110f0179abfb6d9b82d672557d55ff84
check_hash tests/gsm23236/gsm23236_test.ok 6fa94b9faaf62842f5850661ac6042d03530f8e8794c93f69598c670a7312f37
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
binary=$(mktemp "${TMPDIR:-/tmp}/gsm23236-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gsm23236_vector.c" \
	"$gsm" "$core" -ltalloc \
	-Wl,-rpath,"$libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gsm23236/gsm23236_test.ok"
