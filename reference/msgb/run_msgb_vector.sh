#!/usr/bin/env sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified byte-for-byte against upstream 950430e829a3dc1d162aa241bc0505745c5a7311 on 2026-10-08.
test "$(sha256sum "$reference_dir/src/core/msgb.c" | cut -d ' ' -f1)" = eaf5f2fe4f3cfa3cf44de33d5c47804990e120e5ab8f8d17e797bd08af437ee1 || { echo 'Reference mismatch: src/core/msgb.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/msgb.h" | cut -d ' ' -f1)" = 26ab5bf78406a29d60d8079ae5516b553234b433aec100c958cdc50bb80f5f4a || { echo 'Reference mismatch: include/osmocom/core/msgb.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/tests/msgb/msgb_test.c" | cut -d ' ' -f1)" = 5eac93541a8d80107b5512100bbc9743fbc263c4145da73a36deb91d23cb4bf4 || { echo 'Reference mismatch: tests/msgb/msgb_test.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/panic.c" | cut -d ' ' -f1)" = 8686ec3d0283bba6a0d8e168662f24f7a290f88e703691619d1f7d285827eb18 || { echo 'Reference mismatch: src/core/panic.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/panic.h" | cut -d ' ' -f1)" = f2f6466f274bd045200626ade442479706ece3c9c4ba75164628ae84b7cb37fd || { echo 'Reference mismatch: include/osmocom/core/panic.h' >&2; exit 1; }
harness=msgb_vector.c
case "${1:-}" in
    '') ;;
    --layers) harness=msgb_layers_vector.c ;;
    --adapters) harness=msgb_adapters_vector.c ;;
    --resize) harness=msgb_resize_vector.c ;;
    *) echo 'Usage: run_msgb_vector.sh [--layers|--resize|--adapters]' >&2; exit 1 ;;
esac
if [ "$harness" = msgb_layers_vector.c ]; then
    test "$(sha256sum "$reference_dir/include/osmocom/core/bit16gen.h" | cut -d ' ' -f1)" = 927d65fa34c9cc59cf777fadabea4a7d370f934eebf85f24c0c25a540a8df895 || { echo 'Reference mismatch: include/osmocom/core/bit16gen.h' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/bit32gen.h" | cut -d ' ' -f1)" = dac4631b3a38d3fba61af5c739cf1823d2a5cd2f460a511aeef04412601a4cf0 || { echo 'Reference mismatch: include/osmocom/core/bit32gen.h' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/bitXXgen.h.tpl" | cut -d ' ' -f1)" = 35109704fc81fd83d2a75072f1efe1df8fafbc1775c1a5114d2fb892441f02a1 || { echo 'Reference mismatch: include/osmocom/core/bitXXgen.h.tpl' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/Makefile.am" | cut -d ' ' -f1)" = 3474af2d135857e1da1814fbae33aff8b24292d7d740041d4454cdf72c49e83e || { echo 'Reference mismatch: include/osmocom/core/Makefile.am' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/bits.h" | cut -d ' ' -f1)" = 922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7 || { echo 'Reference mismatch: include/osmocom/core/bits.h' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)" = 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d || { echo 'Reference mismatch: include/osmocom/core/utils.h' >&2; exit 1; }
fi
if [ "$harness" = msgb_resize_vector.c ]; then
    test "$(sha256sum "$reference_dir/src/core/logging.c" | cut -d ' ' -f1)" = 13a0adeda85a762695f4b8c338f5b272b81774a15780c218ac4ccd13b21b2d0d || { echo 'Reference mismatch: src/core/logging.c' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/logging.h" | cut -d ' ' -f1)" = 7454572bcd4655f9cd87d8fe64e87f10bc17c322a14669982dfb62bcff002c55 || { echo 'Reference mismatch: include/osmocom/core/logging.h' >&2; exit 1; }
    test "$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)" = 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d || { echo 'Reference mismatch: include/osmocom/core/utils.h' >&2; exit 1; }
fi
if [ "$harness" = msgb_adapters_vector.c ]; then
    # Additional dependencies verified byte-for-byte against the pinned revision on 2026-10-09.
    test "$(sha256sum "$reference_dir/src/core/utils.c" | cut -d ' ' -f1)" = a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
    test "$(sha256sum "$reference_dir/include/osmocom/core/linuxlist.h" | cut -d ' ' -f1)" = 9015f71c9e30755d48f8f8597c8a5362cb98f5acccf5149b9cf6a5a6b508e39f
    test "$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)" = 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d
fi
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
test -f "$library" || { echo 'Required local C library is missing' >&2; exit 1; }
binary=$(mktemp "${TMPDIR:-/tmp}/msgb-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT
trap 'exit 1' HUP INT TERM
c_standard=c11
if [ "$harness" = msgb_adapters_vector.c ]; then c_standard=gnu11; fi
cc -std="$c_standard" -Wall -Wextra -Werror -isystem "$reference_dir/include" \
    "$script_dir/$harness" "$library" -ltalloc \
    -Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= LD_AUDIT= "$binary"
