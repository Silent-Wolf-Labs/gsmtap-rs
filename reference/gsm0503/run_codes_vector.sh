#!/usr/bin/env sh
# Writes the convolutional-code and parity fixture to standard output. With
# --check-backends, instead checks that the scalar, SSE and AVX correlation decoders
# all produce that same output.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
# Upstream 950430e829a3dc1d162aa241bc0505745c5a7311 doesn't track gsm0503_conv.c,
# gsm0503.h or the crc*gen sources and headers, which the build generates. Their
# generators are verified against it.
check_hash() {
    actual=$(sha256sum "$reference_dir/$1" | cut -d ' ' -f1)
    [ "$actual" = "$2" ] || { echo "C reference hash mismatch: $1" >&2; exit 1; }
}
check_hash src/coding/gsm0503_parity.c 0231f71050dbfc51837859f713500a031d8d4de600ecf6b23991126dcdc817af
check_hash include/osmocom/coding/gsm0503_parity.h b4ba3c8c838db8aee690caa121f1f61a81be63a6c3c901c0136c60408a7f633e
check_hash src/core/conv.c 3170277ff8d9e3cd273fb2a5216897a99c77481d96b12ca9bbdaf2894aba1fed
check_hash src/core/conv_acc.c 682d97ef5160498ff22a5abdebda4508a66950751c4e64d67ecb4f68c8e08d18
check_hash src/core/conv_acc_generic.c 383fc57c0c61cf80f93b92b53b474fc7f292c9d5d7d607ea74cc18017097181c
check_hash src/core/conv_acc_sse.c a8bfcfe5cbec73e3da830bd54f0f55d98b6f5fdc09fa1ea3a11ceca635882b2a
check_hash src/core/conv_acc_sse_avx.c 51bbfbcd4a58e27b443d525cc21b7075c12e76bcdfd7d7ee2d83b11e1e43002f
check_hash src/core/conv_acc_sse_impl.h 4bffeb15b1df870e40bd36329e0f8563e762f6f33b7baf4f7c436178ef92ec4f
check_hash include/osmocom/core/conv.h c670a76690e86a122929c84ca41223b5b8774f0d0c25479226706135630d4132
check_hash src/core/crcXXgen.c.tpl 003824c93a86def704dfe0d7e835b97b1590ca92b7e34d6eeaf30a0c470a6c59
check_hash include/osmocom/core/crcXXgen.h.tpl 1945d6ed8688dd366cfb715b8d57c24b309db21c5ac56d4787a7e9339d650326
check_hash include/osmocom/core/crcgen.h 0a2dbb97b3fafa7293b172cc35120228c6a278b26e440191859a0791a41af67b
check_hash utils/conv_codes_gsm.py abf57fabd6d24b3697888d762fd425178c45c2cc2c9d4d77eb4be72504dfb4df
check_hash utils/conv_gen.py 678ed5ee60fb9b88090092fe3038b0433975153a5475686d7b64ae46490ccff3
check_hash tests/conv/conv_gsm0503_test.c e1bbb13ee0f2cc889b3f73de0124eb767814b73c982702c060d6ac17c7d8fba5
check_hash tests/conv/conv_gsm0503_test.ok 3c5d405695fa5059a356081611bd337d72ba08a13534ca671e67766a27aaec1c
check_hash tests/conv/conv.c 02518ac877c7a6f1c5b3922c80e8a5201dbba15514a6d0f6c6632394cd6d6650
check_hash tests/conv/conv.h 581ad648a971947e88aea7c7e72a24998ee838baa636abb173f832bd5d306a4d
check_hash src/gsm/gsm0503_conv.c 03b21bce7e625a54960846dbf1ad48f389ad49b4c2673c614706b7cd372d028c
check_hash include/osmocom/gsm/gsm0503.h ad86e252c3d31bad9e7864239c906ff07a8146930a6d14875fb8018a052d160a
check_hash src/core/crc8gen.c 22e2a2967d8b2f7f0a3192429f7374d4ff85347d8f076879df90b3f249f3d7b3
check_hash include/osmocom/core/crc8gen.h 224db44da888a8c5b2a7de0f3291836f9352968f27960672d129d403e5fb0eb9
check_hash src/core/crc16gen.c 43bae71b01fb77bc6fe85b022278bb119d45f7abe7246aec54319788ab9e7137
check_hash include/osmocom/core/crc16gen.h 10c853bcab13db78b0ac70de66ebd20cd43abac51b88864dc51c036b7062d904
check_hash src/core/crc64gen.c 53b2bd8694e13359490a7680475a8b5e82c21bf4fdcc073bc75cd3dfc74f9fe3
check_hash include/osmocom/core/crc64gen.h 66fb92b362c7e177099189477f20af50217bcf1775037178c58072ad87313720
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
work=$(mktemp -d "${TMPDIR:-/tmp}/codes-vector.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

link() {
    cc -std=gnu11 -O1 -Wall -Wextra -Werror "$@" \
        "$coding/libosmocoding.so" "$gsm/libosmogsm.so" "$codec/libosmocodec.so" "$core/libosmocore.so" \
        -Wl,-rpath,"$LD_LIBRARY_PATH"
}
expected="$reference_dir/tests/conv/conv_gsm0503_test.ok"

link -isystem "$reference_dir/include" "$script_dir/codes_vector.c" -o "$work/linked"
if [ "${1:-}" != --check-backends ]; then
    "$work/linked" "$expected"
    exit 0
fi

"$work/linked" "$expected" > "$work/linked.ndjson"
for backend in gen sse sse_avx; do
    # Existing upstream warnings, not harness warnings.
    link -isystem "$reference_dir/include" -I"$reference_dir" -I"$reference_dir/src/core" \
        -Wno-error -w -DCODES_BACKEND="$backend" -mssse3 -msse4.1 -mavx2 \
        "$script_dir/codes_vector.c" \
        "$reference_dir/src/core/conv.c" "$reference_dir/src/core/conv_acc_generic.c" \
        "$reference_dir/src/core/conv_acc_sse.c" "$reference_dir/src/core/conv_acc_sse_avx.c" \
        -o "$work/$backend"
    # The harness calls osmo_conv_decode itself, so a definition in the binary
    # takes the call instead of the library's.
    nm "$work/$backend" | grep -q ' T osmo_conv_decode$' \
        || { echo "$backend: library decoder was not replaced" >&2; exit 1; }
    if "$work/$backend" "$expected" | cmp -s "$work/linked.ndjson" -; then
        echo "$backend: identical"
    else
        echo "$backend: differs from the linked library" >&2
        "$work/$backend" "$expected" > "$work/$backend.ndjson"
        diff "$work/linked.ndjson" "$work/$backend.ndjson" | grep -c '^>' >&2
        failed=1
    fi
done
exit "${failed:-0}"
