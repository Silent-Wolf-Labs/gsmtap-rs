#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash src/gsm/auth_milenage.c 102e3d9ca681f61d58fa5013f38f6ce6df446e70c038c28d14283d8af7dd8512
check_hash src/gsm/auth_xor.c 751a52fd893ed830b51be122e7d88f5bcfcb2fb8228c1c8060fc6c1377b09335
check_hash src/gsm/milenage/milenage.c 15b2193d551e5815103e92063a628c19a59da3feedc96ff13bbb7cc852dc01cd
check_hash src/gsm/milenage/milenage.h e0e6ae93357d6699cec942285726503ab0b79e82c6004c9aa53ce7d124732976
check_hash src/gsm/milenage/aes-encblock.c 9d84051c6ca43c14748ec703ae24dcf38f1138d02add6c86fc2958744b85042d
check_hash src/gsm/milenage/aes-internal.c b982329678484dde68b5799fa5c2310e127e550fbb40a8744066adb2d2093d65
check_hash src/gsm/milenage/aes-internal-enc.c a88a231c9b337815e8d4130b70c8ce43ed205a3b60719e88a2c2282d6de01cb1
check_hash src/gsm/milenage/aes_i.h 81b6c47592688528f0f6083f33252898a2c7a2f91f71a3dbf942c288120c4c78
check_hash include/osmocom/crypt/auth.h 97e10cca85377d6960921dbfe798db2c69a2a4d288bc69e7ee32af517b385162
check_hash tests/auth/milenage_test.c 2a9dae4dd7f2f0ce21b8d4fa41f8e5b545beecc755ffa643d780ef93f10247e7
check_hash tests/auth/milenage_test.ok fa81b4cca5ca991ec05b6582f88f6dab93f4e76d20142b5489288125019416d8
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if either test fails.
[ -f "$core" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/auth-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/auth_vector.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$reference_dir/src/gsm/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/auth/milenage_test.ok"
