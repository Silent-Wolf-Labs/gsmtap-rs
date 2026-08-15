#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=$(mktemp "${TMPDIR:-/tmp}/gsmtap-makemsg-ex-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror \
	"$script_dir/gsmtap_makemsg_ex_vector.c" \
	$(pkg-config --cflags --libs libosmocore) \
	-o "$binary"

"$binary"
