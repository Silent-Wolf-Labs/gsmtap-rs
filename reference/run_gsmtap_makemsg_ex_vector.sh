#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/gsmtap-makemsg-ex-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

# libosmocore's public headers include an intentionally empty prefetch()
# fallback. Keep warnings enabled while silencing only that upstream warning.
cc -std=c11 -Wall -Wextra -Wno-unused-parameter \
	"$script_dir/gsmtap_makemsg_ex_vector.c" \
	$(pkg-config --cflags --libs libosmocore) \
	-o "$binary"

"$binary" "$@"
