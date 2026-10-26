#!/usr/bin/env sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# These files were verified byte-for-byte at revision 950430e829a3dc1d162aa241bc0505745c5a7311 on 2026-10-09.
test "$(sha256sum "$reference_dir/src/core/prim.c" | cut -d ' ' -f1)" = 40359a64f0963d34e344482dc5d6f68c9229eb44c93318a04ac517fe41e85d35 || { echo 'Reference mismatch: src/core/prim.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/prim.h" | cut -d ' ' -f1)" = 4c1f9789777d22e5d646ab9ae515571eedd1c7b7487a491d897e2be982a3da58 || { echo 'Reference mismatch: include/osmocom/core/prim.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/utils.c" | cut -d ' ' -f1)" = a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4 || { echo 'Reference mismatch: src/core/utils.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)" = 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d || { echo 'Reference mismatch: include/osmocom/core/utils.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/msgb.c" | cut -d ' ' -f1)" = eaf5f2fe4f3cfa3cf44de33d5c47804990e120e5ab8f8d17e797bd08af437ee1 || { echo 'Reference mismatch: src/core/msgb.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/msgb.h" | cut -d ' ' -f1)" = 26ab5bf78406a29d60d8079ae5516b553234b433aec100c958cdc50bb80f5f4a || { echo 'Reference mismatch: include/osmocom/core/msgb.h' >&2; exit 1; }
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
test -f "$library" || { echo 'Required local C library is missing' >&2; exit 1; }
binary=$(mktemp "${TMPDIR:-/tmp}/prim-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT
trap 'exit 1' HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -isystem "$reference_dir/include" \
    "$script_dir/prim_vector.c" "$library" -ltalloc \
    -Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= LD_AUDIT= "$binary" 40359a64f0963d34e344482dc5d6f68c9229eb44c93318a04ac517fe41e85d35 4c1f9789777d22e5d646ab9ae515571eedd1c7b7487a491d897e2be982a3da58 a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d eaf5f2fe4f3cfa3cf44de33d5c47804990e120e5ab8f8d17e797bd08af437ee1 26ab5bf78406a29d60d8079ae5516b553234b433aec100c958cdc50bb80f5f4a
