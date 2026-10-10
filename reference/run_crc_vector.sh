#!/usr/bin/env sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
# Source/templates verified at 950430e829a3dc1d162aa241bc0505745c5a7311 on 2026-10-09.
# Generated files were checked byte-for-byte against the verified templates.
test "$(sha256sum "$reference_dir/src/core/crc16.c" | cut -d ' ' -f1)" = c59ec3c0353c18ab72903ff831829cc8775e102b33d250cf552fb59e35b127c3 || { echo 'Reference mismatch: src/core/crc16.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crc16.h" | cut -d ' ' -f1)" = fc6fc2a4dba7d60bae4b5406186f2ef0f6b4626b3599d12b72953382b7bb2ce9 || { echo 'Reference mismatch: include/osmocom/core/crc16.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/crcXXgen.c.tpl" | cut -d ' ' -f1)" = 003824c93a86def704dfe0d7e835b97b1590ca92b7e34d6eeaf30a0c470a6c59 || { echo 'Reference mismatch: src/core/crcXXgen.c.tpl' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crcXXgen.h.tpl" | cut -d ' ' -f1)" = 1945d6ed8688dd366cfb715b8d57c24b309db21c5ac56d4787a7e9339d650326 || { echo 'Reference mismatch: include/osmocom/core/crcXXgen.h.tpl' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crcgen.h" | cut -d ' ' -f1)" = 0a2dbb97b3fafa7293b172cc35120228c6a278b26e440191859a0791a41af67b || { echo 'Reference mismatch: include/osmocom/core/crcgen.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/bits.h" | cut -d ' ' -f1)" = 922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7 || { echo 'Reference mismatch: include/osmocom/core/bits.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/Makefile.am" | cut -d ' ' -f1)" = 1804d0b242711232b424a76c56692edd05025dfb6be7e996e528cad3a2da5db7 || { echo 'Reference mismatch: src/core/Makefile.am' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/crc8gen.c" | cut -d ' ' -f1)" = 22e2a2967d8b2f7f0a3192429f7374d4ff85347d8f076879df90b3f249f3d7b3 || { echo 'Reference mismatch: src/core/crc8gen.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crc8gen.h" | cut -d ' ' -f1)" = 224db44da888a8c5b2a7de0f3291836f9352968f27960672d129d403e5fb0eb9 || { echo 'Reference mismatch: include/osmocom/core/crc8gen.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/crc16gen.c" | cut -d ' ' -f1)" = 43bae71b01fb77bc6fe85b022278bb119d45f7abe7246aec54319788ab9e7137 || { echo 'Reference mismatch: src/core/crc16gen.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crc16gen.h" | cut -d ' ' -f1)" = 10c853bcab13db78b0ac70de66ebd20cd43abac51b88864dc51c036b7062d904 || { echo 'Reference mismatch: include/osmocom/core/crc16gen.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/crc32gen.c" | cut -d ' ' -f1)" = b38571941727b1aa9dd396d60cc448ac4a34e2562fe69cb6e1617f0335386f8c || { echo 'Reference mismatch: src/core/crc32gen.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crc32gen.h" | cut -d ' ' -f1)" = 012a9893ef2527cd2e1224d49aa97140d180c997140c9e1d2720c7e097da2baf || { echo 'Reference mismatch: include/osmocom/core/crc32gen.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/src/core/crc64gen.c" | cut -d ' ' -f1)" = 53b2bd8694e13359490a7680475a8b5e82c21bf4fdcc073bc75cd3dfc74f9fe3 || { echo 'Reference mismatch: src/core/crc64gen.c' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/crc64gen.h" | cut -d ' ' -f1)" = 66fb92b362c7e177099189477f20af50217bcf1775037178c58072ad87313720 || { echo 'Reference mismatch: include/osmocom/core/crc64gen.h' >&2; exit 1; }
test "$(sha256sum "$reference_dir/include/osmocom/core/Makefile.am" | cut -d ' ' -f1)" = 3474af2d135857e1da1814fbae33aff8b24292d7d740041d4454cdf72c49e83e || { echo 'Reference mismatch: include/osmocom/core/Makefile.am' >&2; exit 1; }
mode=${1:-byte}
case "$mode" in byte|generic|--audit) ;; *) echo 'Usage: run_crc_vector.sh [byte|generic|--audit]' >&2; exit 1 ;; esac
binary=$(mktemp "${TMPDIR:-/tmp}/crc-vector.XXXXXX")
audit_log=$(mktemp "${TMPDIR:-/tmp}/crc-audit.XXXXXX")
trap 'rm -f "$binary" "$audit_log"' EXIT
trap 'exit 1' HUP INT TERM
if [ "$mode" = --audit ]; then
    cc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all \
        -isystem "$reference_dir/include" -I"$reference_dir" "$script_dir/crc_vector.c" \
        "$reference_dir/src/core/crc16.c" "$reference_dir/src/core/crc8gen.c" \
        "$reference_dir/src/core/crc16gen.c" "$reference_dir/src/core/crc32gen.c" \
        "$reference_dir/src/core/crc64gen.c" -o "$binary"
    for args in '8 8 1' '16 16 1' '32 31 1' '64 63 1' '32 32 0' '64 64 0' '32 32 -1' '64 64 -1'; do
        "$binary" audit $args
    done
    for args in '32 32 1' '64 64 1' '8 0 1' '16 -2147483648 0'; do
        if "$binary" audit $args 2>"$audit_log"; then
            echo "Expected undefined-behavior diagnostic: $args" >&2; exit 1
        fi
        grep -Eq 'shift exponent|signed integer overflow' "$audit_log"
        cat "$audit_log" >&2
    done
    # The entire golden domain must be sanitizer-clean, too.
    "$binary" byte audit >/dev/null
    "$binary" generic audit >/dev/null
    echo 'CRC audit: defined domain clean; excluded shift/descriptor cases diagnosed' >&2
    exit 0
fi
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
test -f "$library" || { echo 'Required local C library is missing' >&2; exit 1; }
cc -std=c11 -Wall -Wextra -Werror -isystem "$reference_dir/include" \
    "$script_dir/crc_vector.c" "$library" -Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= LD_AUDIT= "$binary" "$mode" 'src/core/crc16.c=c59ec3c0353c18ab72903ff831829cc8775e102b33d250cf552fb59e35b127c3;include/osmocom/core/crc16.h=fc6fc2a4dba7d60bae4b5406186f2ef0f6b4626b3599d12b72953382b7bb2ce9;src/core/crcXXgen.c.tpl=003824c93a86def704dfe0d7e835b97b1590ca92b7e34d6eeaf30a0c470a6c59;include/osmocom/core/crcXXgen.h.tpl=1945d6ed8688dd366cfb715b8d57c24b309db21c5ac56d4787a7e9339d650326;include/osmocom/core/crcgen.h=0a2dbb97b3fafa7293b172cc35120228c6a278b26e440191859a0791a41af67b;include/osmocom/core/bits.h=922f568b46a84f36a2c6208308a71ceb505088b217e7105f254725f029c297f7;src/core/Makefile.am=1804d0b242711232b424a76c56692edd05025dfb6be7e996e528cad3a2da5db7;src/core/crc8gen.c=22e2a2967d8b2f7f0a3192429f7374d4ff85347d8f076879df90b3f249f3d7b3;include/osmocom/core/crc8gen.h=224db44da888a8c5b2a7de0f3291836f9352968f27960672d129d403e5fb0eb9;src/core/crc16gen.c=43bae71b01fb77bc6fe85b022278bb119d45f7abe7246aec54319788ab9e7137;include/osmocom/core/crc16gen.h=10c853bcab13db78b0ac70de66ebd20cd43abac51b88864dc51c036b7062d904;src/core/crc32gen.c=b38571941727b1aa9dd396d60cc448ac4a34e2562fe69cb6e1617f0335386f8c;include/osmocom/core/crc32gen.h=012a9893ef2527cd2e1224d49aa97140d180c997140c9e1d2720c7e097da2baf;src/core/crc64gen.c=53b2bd8694e13359490a7680475a8b5e82c21bf4fdcc073bc75cd3dfc74f9fe3;include/osmocom/core/crc64gen.h=66fb92b362c7e177099189477f20af50217bcf1775037178c58072ad87313720;include/osmocom/core/Makefile.am=3474af2d135857e1da1814fbae33aff8b24292d7d740041d4454cdf72c49e83e'
