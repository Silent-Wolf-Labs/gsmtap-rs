#!/usr/bin/env sh
# Writes the fixture to standard output. With --check-backends, instead checks that
# the scalar, SSE and AVX decoders all produce that same output.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Upstream 950430e829a3dc1d162aa241bc0505745c5a7311 doesn't track gsm0503_conv.c or
# crc64gen.c, which the build generates. Their generators are verified against it.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/coding/gsm0503_coding.c 98ad47a01b09fa8b9a66bdf51be5d622a3b9672a6dc63260e31c9c88588198d7
check_hash src/coding/gsm0503_interleaving.c 10ecea49bbf3d5f864e821bba9f7f35bf747df1ea3050060cf77220cf2a9885d
check_hash src/coding/gsm0503_mapping.c 82eb0428a40030139a8423bbbc504b4d00c08cef0e8d353f6d052b2c67992a38
check_hash src/coding/gsm0503_parity.c 0231f71050dbfc51837859f713500a031d8d4de600ecf6b23991126dcdc817af
check_hash include/osmocom/coding/gsm0503_coding.h 89c332fdb974646f0d973157809bd96903c45a2d6b85b5e413f2a9d9c74cf3b8
check_hash include/osmocom/coding/gsm0503_interleaving.h eb391c2c0a87803807dbd26cc002d52b77fe4718a6a899166d116004fb1be3aa
check_hash include/osmocom/coding/gsm0503_mapping.h 49707b9c70aac0645d906b27c3637335404879ed94b8f5d2d56e7dcba833e01e
check_hash include/osmocom/coding/gsm0503_parity.h b4ba3c8c838db8aee690caa121f1f61a81be63a6c3c901c0136c60408a7f633e
check_hash src/core/conv.c 3170277ff8d9e3cd273fb2a5216897a99c77481d96b12ca9bbdaf2894aba1fed
check_hash src/core/conv_acc.c 682d97ef5160498ff22a5abdebda4508a66950751c4e64d67ecb4f68c8e08d18
check_hash src/core/conv_acc_generic.c 383fc57c0c61cf80f93b92b53b474fc7f292c9d5d7d607ea74cc18017097181c
check_hash src/core/conv_acc_sse.c a8bfcfe5cbec73e3da830bd54f0f55d98b6f5fdc09fa1ea3a11ceca635882b2a
check_hash src/core/conv_acc_sse_avx.c 51bbfbcd4a58e27b443d525cc21b7075c12e76bcdfd7d7ee2d83b11e1e43002f
check_hash src/core/conv_acc_sse_impl.h 4bffeb15b1df870e40bd36329e0f8563e762f6f33b7baf4f7c436178ef92ec4f
check_hash src/core/crcXXgen.c.tpl 003824c93a86def704dfe0d7e835b97b1590ca92b7e34d6eeaf30a0c470a6c59
check_hash utils/conv_codes_gsm.py abf57fabd6d24b3697888d762fd425178c45c2cc2c9d4d77eb4be72504dfb4df
check_hash utils/conv_gen.py 678ed5ee60fb9b88090092fe3038b0433975153a5475686d7b64ae46490ccff3
check_hash tests/coding/coding_test.c bbc33be9692c8f3777807c6c3b26335ddd3861187a1c7f6454cdb7b7fc0dbb6b
check_hash tests/coding/coding_test.ok 41ec1663f121fb5ad98a6210222a484e4c890be144859e025cfddbe4172e4514
check_hash src/gsm/gsm0503_conv.c 03b21bce7e625a54960846dbf1ad48f389ad49b4c2673c614706b7cd372d028c
check_hash src/core/crc64gen.c 53b2bd8694e13359490a7680475a8b5e82c21bf4fdcc073bc75cd3dfc74f9fe3
for directory in core codec gsm coding; do
    make -C "$reference_dir/src/$directory" >/dev/null
done
core="$reference_dir/src/core/.libs"
codec="$reference_dir/src/codec/.libs"
gsm="$reference_dir/src/gsm/.libs"
coding="$reference_dir/src/coding/.libs"
for library in "$core/libosmocore.so" "$codec/libosmocodec.so" "$gsm/libosmogsm.so" "$coding/libosmocoding.so"; do
    [ -f "$library" ] || { echo 'Canonical C library is unavailable' >&2; exit 1; }
done
export LD_LIBRARY_PATH="$coding:$gsm:$codec:$core"
export LD_PRELOAD=
work=$(mktemp -d "${TMPDIR:-/tmp}/xcch-vector.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

link() {
    cc -std=gnu11 -O1 -Wall -Wextra -Werror "$@" \
        "$coding/libosmocoding.so" "$gsm/libosmogsm.so" "$codec/libosmocodec.so" "$core/libosmocore.so" \
        -Wl,-rpath,"$LD_LIBRARY_PATH"
}
expected="$reference_dir/tests/coding/coding_test.ok"

link -isystem "$reference_dir/include" "$script_dir/xcch_vector.c" -o "$work/linked"
if [ "${1:-}" != --check-backends ]; then
    "$work/linked" "$expected"
    exit 0
fi

"$work/linked" "$expected" > "$work/linked.ndjson"
for backend in gen sse sse_avx; do
    # Existing upstream warnings, not harness warnings.
    link -isystem "$reference_dir/include" -I"$reference_dir" -I"$reference_dir/src/core" \
        -Wno-error -w -DXCCH_BACKEND="$backend" -mssse3 -msse4.1 -mavx2 \
        "$script_dir/xcch_vector.c" \
        "$reference_dir/src/core/conv.c" "$reference_dir/src/core/conv_acc_generic.c" \
        "$reference_dir/src/core/conv_acc_sse.c" "$reference_dir/src/core/conv_acc_sse_avx.c" \
        -o "$work/$backend"
    # The library must call the forced decoder in this binary, not its own.
    LD_DEBUG=bindings "$work/$backend" "$expected" 2>&1 >/dev/null \
        | grep -q "libosmocoding.*to $work/$backend.*osmo_conv_decode" \
        || { echo "$backend: library decoder was not replaced" >&2; exit 1; }
    if cmp -s "$work/linked.ndjson" - <<RESULT
$("$work/$backend" "$expected")
RESULT
    then
        echo "$backend: identical"
    else
        echo "$backend: differs from the linked library" >&2
        "$work/$backend" "$expected" > "$work/$backend.ndjson"
        diff "$work/linked.ndjson" "$work/$backend.ndjson" | grep -c '^>' >&2
        cp "$work/$backend.ndjson" "${XCCH_KEEP_DIR:-$work}/" 2>/dev/null || true
        failed=1
    fi
done
exit "${failed:-0}"
