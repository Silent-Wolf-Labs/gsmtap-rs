#!/usr/bin/env sh
# Writes the fixture to standard output. Upstream's own TLV test must also pass.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/tlv_parser.c 8e00834d2e87e36a73bc5c6b3b70f537d4c1eb3828ba2b19745fc79b28f9098d
check_hash include/osmocom/gsm/tlv.h 2b932ecf8be07c0edee1fc85cb52605baea2fbcf7ffd2617a5244b7d9c16428f
check_hash tests/tlv/tlv_test.c 7a735bdede24b4c2d89d6052c5a7510c3927151a9b7f643519cb0815d787d3d2
check_hash tests/tlv/tlv_test.ok 4f81f8ff73910e46de08470ac68ca38bd917634185069f2786ef6c45b22fa3af
check_hash src/core/msgb.c eaf5f2fe4f3cfa3cf44de33d5c47804990e120e5ab8f8d17e797bd08af437ee1
check_hash include/osmocom/core/msgb.h 26ab5bf78406a29d60d8079ae5516b553234b433aec100c958cdc50bb80f5f4a
check_hash src/core/logging.c 13a0adeda85a762695f4b8c338f5b272b81774a15780c218ac4ccd13b21b2d0d
check_hash include/osmocom/core/logging.h 7454572bcd4655f9cd87d8fe64e87f10bc17c322a14669982dfb62bcff002c55
check_hash src/core/utils.c a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
for directory in core gsm; do
    make -C "$reference_dir/src/$directory" >/dev/null
done
core="$reference_dir/src/core/.libs"
gsm="$reference_dir/src/gsm/.libs"
for library in "$core/libosmocore.so" "$gsm/libosmogsm.so"; do
    [ -f "$library" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
done
export LD_LIBRARY_PATH="$gsm:$core"
export LD_PRELOAD=
work=$(mktemp -d "${TMPDIR:-/tmp}/tlv-vector.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

link() {
    cc -std=gnu11 -O1 -Wall -Wextra -Werror -isystem "$reference_dir/include" "$@" \
        "$gsm/libosmogsm.so" "$core/libosmocore.so" -ltalloc \
        -Wl,-rpath,"$LD_LIBRARY_PATH"
}

link -Wno-error -w "$reference_dir/tests/tlv/tlv_test.c" -o "$work/upstream"
"$work/upstream" > "$work/upstream.out" 2>/dev/null
cmp -s "$work/upstream.out" "$reference_dir/tests/tlv/tlv_test.ok" \
    || { echo 'Upstream tlv_test output differs from tlv_test.ok' >&2; exit 1; }

link "$script_dir/tlv_vector.c" -o "$work/harness"
"$work/harness"
