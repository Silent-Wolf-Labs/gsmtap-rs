#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
# Verified byte-for-byte against pinned upstream; the reference has no .git.
source_hash=$(sha256sum "$reference_dir/src/core/bitvec.c" | cut -d ' ' -f1)
header_hash=$(sha256sum "$reference_dir/include/osmocom/core/bitvec.h" | cut -d ' ' -f1)
test_hash=$(sha256sum "$reference_dir/tests/bitvec/bitvec_test.c" | cut -d ' ' -f1)
if [ "$source_hash" != 789ef706bc0c71f497400f831e920c9efad324dfaea99b13292fca717b4f480e ] ||
   [ "$header_hash" != db895f6eb8bfada1d6764248d122128b0a0bb530ec5f4a198953f56f223215b6 ] ||
   [ "$test_hash" != 5b4a1a3f8c5bafd4113f7b4634f3c80061c274356e05c328d270fb40dfb007f5 ]; then
    echo 'Local bitvec source does not match the verified revision' >&2
    exit 1
fi
harness=bitvec_vector.c
case "${1:-}" in
    '') ;;
    --buffers) harness=bitvec_buffers_vector.c ;;
    --operations) harness=bitvec_operations_vector.c ;;
    --adapters) harness=bitvec_adapters_vector.c ;;
    *) echo 'Usage: run_bitvec_vector.sh [--buffers|--operations|--adapters]' >&2; exit 1 ;;
esac
if [ "$harness" = bitvec_adapters_vector.c ]; then
    hex_source_hash=$(sha256sum "$reference_dir/src/core/utils.c" | cut -d ' ' -f1)
    hex_header_hash=$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)
    hex_test_hash=$(sha256sum "$reference_dir/tests/utils/utils_test.c" | cut -d ' ' -f1)
    if [ "$hex_source_hash" != a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4 ] ||
       [ "$hex_header_hash" != 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d ] ||
       [ "$hex_test_hash" != 7666972f9755fc95c69b2740beb6292a9ca28d00dc7f4cc1f55b83c9615bc60d ]; then
        echo 'Local hex parser does not match the verified revision' >&2
        exit 1
    fi
fi
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
test -f "$library" || { echo 'Required local C library is missing' >&2; exit 1; }
binary=$(mktemp "${TMPDIR:-/tmp}/bitvec-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT
trap 'exit 1' HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -I"$reference_dir/include" \
    "$script_dir/$harness" "$library" \
    -Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= LD_AUDIT= "$binary"
