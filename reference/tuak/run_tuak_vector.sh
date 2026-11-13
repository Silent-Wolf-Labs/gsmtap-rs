#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/gsm/auth_tuak.c ee45208351b5950bdb8285e93635284c5b59c7391ff6b2b1ff5a1b9b74481698
check_hash src/gsm/tuak/tuak.c e2d104916f2391f220df810d0e5d0bc145bed3398e2c9414f6a7380d2478110e
check_hash src/gsm/tuak/tuak.h 536be114cb39bf166fe4f9349888f13bd3c324cb4c39d6e1ae9cae95d1e32ac1
check_hash src/gsm/tuak/KeccakP-1600-3gpp.c 723ea4279c476ad0eb6ebf8d27e452c236f497249b41e4e0c69ea24af917eb95
check_hash src/gsm/tuak/KeccakP-1600-3gpp.h cf8bbec20d0fece04f0153cb387ebfb607ad1faafc82b571ffc95d49aea4cb90
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash include/osmocom/crypt/auth.h 97e10cca85377d6960921dbfe798db2c69a2a4d288bc69e7ee32af517b385162
check_hash tests/auth/tuak_test.c 1123419a4c3c66a9010815605ec43253af396f65a20b529d523525c1965de5db
check_hash tests/auth/tuak_test.ok 8082d3c0af0ff18f90fb158d4782551d9c0666eee61017988a0b2466f72c8c01
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/isdn" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
isdn="$reference_dir/src/isdn/.libs/libosmoisdn.so"
# The internal archive the shared libosmogsm is built from, as upstream's tests link.
gsmint="$reference_dir/src/gsm/.libs/libgsmint.a"
# Nothing refers to the object that registers TUAK with the auth core, so it's linked by
# name; the archive alone would leave it out.
tuak="$reference_dir/src/gsm/.libs/auth_tuak.o"
# shellcheck disable=SC2015  # The block is meant to run if any test fails.
[ -f "$core" ] && [ -f "$isdn" ] && [ -f "$gsmint" ] && [ -f "$tuak" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
export LD_LIBRARY_PATH="$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs"
export LD_PRELOAD=
binary=$(mktemp "${TMPDIR:-/tmp}/tuak-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT HUP INT TERM

cc -std=gnu11 -Wall -Wextra -Werror \
	-isystem "$reference_dir/include" \
	"$script_dir/tuak_vector.c" \
	"$tuak" "$gsmint" "$isdn" "$core" -ltalloc \
	-Wl,-rpath,"$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs" \
	-o "$binary"

"$binary" "$reference_dir/tests/auth/tuak_test.c" "$reference_dir/tests/auth/tuak_test.ok"
