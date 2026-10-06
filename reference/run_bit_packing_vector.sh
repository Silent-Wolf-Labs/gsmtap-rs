#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
# Verified byte-for-byte against upstream revision
# 950430e829a3dc1d162aa241bc0505745c5a7311; do not infer revision from parent Git.
source_hash=$(sha256sum "$reference_dir/src/core/bits.c" | cut -d ' ' -f1)
header_hash=$(sha256sum "$reference_dir/include/osmocom/core/bits.h" | cut -d ' ' -f1)
if [ "$source_hash" != bb33ceafed9bc49da6c002804108cdcd23914f6f04921b0c95645cd1ddc9fa1d ] ||
   [ "$header_hash" != 922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7 ]; then
	echo 'Local C source does not match the verified libosmocore revision' >&2
	exit 1
fi

make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
if [ ! -f "$library" ]; then
	echo 'Required local libosmocore shared library is missing' >&2
	exit 1
fi
binary=$(mktemp "${TMPDIR:-/tmp}/bit-packing-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT
trap 'exit 1' HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror \
	-I"$reference_dir/include" "$script_dir/bit_packing_vector.c" \
	"$library" \
	-Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
# Override inherited library overrides so the oracle is the same local build.
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= "$binary"
