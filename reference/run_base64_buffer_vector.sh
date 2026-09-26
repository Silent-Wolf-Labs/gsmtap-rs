#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/base64-buffer-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra \
	-I"$reference_dir/include" \
	"$script_dir/base64_buffer_vector.c" \
	-L"$reference_dir/src/core/.libs" -losmocore \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$@"
