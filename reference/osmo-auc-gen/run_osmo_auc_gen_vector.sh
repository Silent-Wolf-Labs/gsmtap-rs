#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Verified against upstream 950430e829a3dc1d162aa241bc0505745c5a7311.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash utils/osmo-auc-gen.c e03102806fa3a87c36b6672352f75e8fc81a7ae976a6bff0912aec8fb2d60a93
check_hash tests/osmo-auc-gen/osmo-auc-gen_test.sh 6442d28c2cf9c7886d4cfb0d11e70d4e879ced5b829e9a533a0bc3a73e25f91e
check_hash tests/osmo-auc-gen/osmo-auc-gen_test.ok e985d0bc82949514bbbdaeaf4567fc1aa15f822d3ba7256867a5fe9c637caed0
check_hash tests/osmo-auc-gen/osmo-auc-gen_test.err f6aa4a4ea437e5bef9124c3bee63b1ecea86523e00b20df641a2bdb3755a5d5c
check_hash src/gsm/auth_core.c 72ae08633a627641f1f7335c215df6825a53396a4a81dd9156a2c9dcbfc67d52
check_hash src/gsm/auth_milenage.c 102e3d9ca681f61d58fa5013f38f6ce6df446e70c038c28d14283d8af7dd8512
check_hash src/gsm/auth_tuak.c ee45208351b5950bdb8285e93635284c5b59c7391ff6b2b1ff5a1b9b74481698
check_hash src/gsm/auth_xor.c 751a52fd893ed830b51be122e7d88f5bcfcb2fb8228c1c8060fc6c1377b09335
check_hash src/gsm/auth_xor_2g.c b68f47dec119d34a226a9f32119ccfdfb6f432b99ae92be88bfde72b6d339c6b
check_hash src/gsm/auth_comp128v1.c 7a5cd3e98708dee58d33065be015f31b911aa08e61cbd4fd61719bc10e0cf502
check_hash src/gsm/auth_comp128v23.c d10904dc60e8643757c44b295498c0b2624387199fe3161cd01402508a0bb1c8
check_hash include/osmocom/crypt/auth.h 97e10cca85377d6960921dbfe798db2c69a2a4d288bc69e7ee32af517b385162
check_hash src/core/utils.c a5d9d5dc5e4ba23a746257d4b751a6d2dc721a245442be8161d37ceb952fdce4
check_hash src/core/base64.c fe3d63a9ba4579d669faf35c47edd93cdccf1ed139009aab219de2f54b2ca37a
make -C "$reference_dir/src/core" >/dev/null
make -C "$reference_dir/src/isdn" >/dev/null
make -C "$reference_dir/src/gsm" >/dev/null
core="$reference_dir/src/core/.libs/libosmocore.so"
isdn="$reference_dir/src/isdn/.libs/libosmoisdn.so"
gsm="$reference_dir/src/gsm/.libs/libosmogsm.so"
# shellcheck disable=SC2015  # The block is meant to run if any test fails.
[ -f "$core" ] && [ -f "$isdn" ] && [ -f "$gsm" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
libs="$reference_dir/src/gsm/.libs:$reference_dir/src/isdn/.libs:$reference_dir/src/core/.libs"
export LD_LIBRARY_PATH="$libs"
export LD_PRELOAD=
work=$(mktemp -d "${TMPDIR:-/tmp}/osmo-auc-gen-vector.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

# The utility itself, from the checkout's source, without its Makefile's install paths.
cc -std=gnu11 -Wall \
	-isystem "$reference_dir/include" \
	"$reference_dir/utils/osmo-auc-gen.c" \
	"$gsm" "$core" \
	-Wl,-rpath,"$libs" \
	-o "$work/osmo-auc-gen"
cc -std=gnu11 -Wall -Wextra -Werror \
	"$script_dir/osmo_auc_gen_vector.c" \
	-o "$work/osmo_auc_gen_vector"

# Upstream's test, as its testsuite runs it.
test_dir="$reference_dir/tests/osmo-auc-gen"
"$test_dir/osmo-auc-gen_test.sh" "$work/osmo-auc-gen" >"$work/test.ok" 2>"$work/test.err"
cmp -s "$work/test.ok" "$test_dir/osmo-auc-gen_test.ok" || { echo 'osmo-auc-gen_test.ok differs' >&2; exit 1; }
cmp -s "$work/test.err" "$test_dir/osmo-auc-gen_test.err" || { echo 'osmo-auc-gen_test.err differs' >&2; exit 1; }

"$work/osmo_auc_gen_vector" "$work/osmo-auc-gen" \
	"$test_dir/osmo-auc-gen_test.ok" "$test_dir/osmo-auc-gen_test.err"
