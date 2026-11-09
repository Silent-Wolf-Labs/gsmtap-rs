#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/kasumi.c c350c3695551f86355697a478a1997852a0c62f6a302b5022aecb7008ded0992
check_hash src/gsm/a5.c 993833a1a5547e5a6453d21b30fc5263b364e46b42dc15b26e94cc3fb641f5a7
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash include/osmocom/gsm/kasumi.h 1e1a8097af76d9b66e77f0d4a441a88d62aff70b7a3cc46ef7361ed9eae95236
check_hash include/osmocom/gsm/a5.h 0b3be9aaf3ebdd9cdc59d445ae006e3271cf2546af36ca228eb0449c6f941074
check_hash include/osmocom/crypt/auth.h 97e10cca85377d6960921dbfe798db2c69a2a4d288bc69e7ee32af517b385162
check_hash tests/kasumi/kasumi_test.c fe2310052c89c2c3db6b5777f300505f7c85ee274c12ebb95078e3d44c99c552
check_hash tests/kasumi/kasumi_test.ok 4793ccde70e2f9405527f2e78b89c7807a651a5b2c5c193b982a43e39b08b528
check_hash tests/a5/a5_test.c 87eb78647c63679bc8e5559d8453b7231f60a92c1824f80e055ad3ff97fd66b6
check_hash tests/a5/a5_test.ok 0ac437ceb85761c55958731e2d9dfaa10d7fb572c2dc47d685bf874345a363d9
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/isdn" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
isdn="$reference_dir/src/isdn/.libs/libosmoisdn.so"
# The internal archive the shared libosmogsm is built from, as upstream's tests link.
gsmint="$reference_dir/src/gsm/.libs/libgsmint.a"
# shellcheck disable=SC2015  # The block is meant to run if any test fails.
[ -f "$core" ] && [ -f "$isdn" ] && [ -f "$gsmint" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/kasumi-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/kasumi_vector.c" \
	"$gsmint" "$isdn" "$core" -ltalloc \
	-Wl,-rpath,"$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/kasumi/kasumi_test.c" "$reference_dir/tests/kasumi/kasumi_test.ok" \
	"$reference_dir/tests/a5/a5_test.c" "$reference_dir/tests/a5/a5_test.ok"
