#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
make -C "$reference_dir/src/core" >/dev/null
binary=$(mktemp "${TMPDIR:-/tmp}/gsmtap-makemsg-ex-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

# libosmocore's public headers include an intentionally empty prefetch()
# fallback. Keep warnings enabled while silencing only that upstream warning.
cc -std=c11 -pthread -Wall -Wextra -Wno-unused-parameter \
	"$script_dir/gsmtap_makemsg_ex_vector.c" \
	-I"$reference_dir/include" \
	-L"$reference_dir/src/core/.libs" -losmocore \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

LD_LIBRARY_PATH="$reference_dir/src/core/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$binary" "$@"
