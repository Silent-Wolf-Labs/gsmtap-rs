#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/gea.c 963d9c1c5dab194f9a793566c33c45af7a80faea31e13e8a782c20c3fb0e2734
check_hash src/gsm/gprs_gea.c 971497a8c9f5e068bfffebd01c41aa25bea68685699e4e1ff9bfe25f8826e691
check_hash src/gsm/gprs_cipher_core.c 20bf18fc8a625c27bfb6a7d9ccf8722abd0c67d4a0f49efbbf400642dda501aa
check_hash src/gsm/kasumi.c c350c3695551f86355697a478a1997852a0c62f6a302b5022aecb7008ded0992
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash include/osmocom/crypt/gprs_cipher.h 251ce8cae34f36f2ca071923505d63761349941c820ab03af8be3c973fc7f7b4
check_hash include/osmocom/gsm/gea.h ac844dd60df503b8b1cd93b87ce916cfee446d33b528ecc869d7b1d53b1e0fdb
check_hash include/osmocom/gsm/kasumi.h 1e1a8097af76d9b66e77f0d4a441a88d62aff70b7a3cc46ef7361ed9eae95236
check_hash tests/gea/gea_test.c 9d5d87d730e3a853c70dea2335ec350b4f8dfc856bc62908f5b595078729ee4a
check_hash tests/gea/gea_test.ok 50baf1001b3d3bba1c36203924fe66e014e3de308cd15dfeb88d0509d722b09f
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/gea-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/gea_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/gea/gea_test.c" "$reference_dir/tests/gea/gea_test.ok"
