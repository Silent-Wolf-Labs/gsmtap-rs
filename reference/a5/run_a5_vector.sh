#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/a5.c 993833a1a5547e5a6453d21b30fc5263b364e46b42dc15b26e94cc3fb641f5a7
check_hash include/osmocom/gsm/a5.h 0b3be9aaf3ebdd9cdc59d445ae006e3271cf2546af36ca228eb0449c6f941074
check_hash tests/a5/a5_test.c 87eb78647c63679bc8e5559d8453b7231f60a92c1824f80e055ad3ff97fd66b6
check_hash tests/a5/a5_test.ok 0ac437ceb85761c55958731e2d9dfaa10d7fb572c2dc47d685bf874345a363d9
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/a5-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/a5_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/a5/a5_test.ok"
