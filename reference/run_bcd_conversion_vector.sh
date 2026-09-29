#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd)
source_hash=$(sha256sum "$reference_dir/src/core/utils.c" | cut -d ' ' -f1)
header_hash=$(sha256sum "$reference_dir/include/osmocom/core/utils.h" | cut -d ' ' -f1)
test_hash=$(sha256sum "$reference_dir/tests/utils/utils_test.c" | cut -d ' ' -f1)

if [ "$source_hash" != a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4 ] ||
   [ "$header_hash" != 3a66898c8a085693541404cf47a5d18588432fceae668609edcc0ae24219c70d ] ||
   [ "$test_hash" != 7666972f9755fc95c69b2740beb6292a9ca28d00dc7f4cc1f55b83c9615bc60d ]; then
	echo 'Local C source does not match the verified libosmocore revision' >&2
	exit 1
fi

make -C "$reference_dir/src/core" >/dev/null
binary=$(mktemp "${TMPDIR:-/tmp}/bcd-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror \
	-I"$reference_dir/include" \
	"$script_dir/bcd_conversion_vector.c" \
	-L"$reference_dir/src/core/.libs" -losmocore \
	-Wl,-rpath,"$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$@"
