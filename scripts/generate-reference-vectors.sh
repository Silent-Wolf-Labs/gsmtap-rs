#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
output_dir="$repository_root/tests/vectors"

# Each direct-call runner resolves and builds the canonical local C checkout.

mkdir -p "$output_dir"
case_names=$("$repository_root/reference/run_gsmtap_makemsg_ex_vector.sh" --list)
for case_name in $case_names; do
	"$repository_root/reference/run_gsmtap_makemsg_ex_vector.sh" --case "$case_name" \
		> "$output_dir/$case_name.json"
done

bcd_output_dir="$output_dir/bcd"
mkdir -p "$bcd_output_dir"
"$repository_root/reference/run_bcd_conversion_vector.sh" \
	> "$bcd_output_dir/bcd_conversion_vectors.json"

hexparse_output_dir="$output_dir/hexparse"
mkdir -p "$hexparse_output_dir"
"$repository_root/reference/run_hexparse_vector.sh" \
	> "$hexparse_output_dir/hexparse_buffer_vectors.json"

bits_output_dir="$output_dir/bits"
mkdir -p "$bits_output_dir"
"$repository_root/reference/run_bit_packing_vector.sh" \
	> "$bits_output_dir/bit_packing_vectors.json"

"$repository_root/reference/run_bit_packing_ext_vector.sh" \
	> "$bits_output_dir/bit_packing_ext_vectors.json"

base64_output_dir="$output_dir/base64"
mkdir -p "$base64_output_dir"
"$repository_root/reference/run_base64_buffer_vector.sh" > "$base64_output_dir/base64_buffer_vectors.json"

bitvec_output_dir="$output_dir/bitvec"
mkdir -p "$bitvec_output_dir"
"$repository_root/reference/run_bitvec_vector.sh" > "$bitvec_output_dir/bitvec_vectors.json"
"$repository_root/reference/run_bitvec_vector.sh" --buffers > "$bitvec_output_dir/bitvec_buffers_vectors.json"
"$repository_root/reference/run_bitvec_vector.sh" --operations > "$bitvec_output_dir/bitvec_operations_vectors.json"
"$repository_root/reference/run_bitvec_vector.sh" --adapters > "$bitvec_output_dir/bitvec_adapters_vectors.json"

msgb_output_dir="$output_dir/msgb"
mkdir -p "$msgb_output_dir"
"$repository_root/reference/run_msgb_vector.sh" > "$msgb_output_dir/msgb_vectors.json"
"$repository_root/reference/run_msgb_vector.sh" --layers > "$msgb_output_dir/msgb_layers_vectors.json"
"$repository_root/reference/run_msgb_vector.sh" --resize > "$msgb_output_dir/msgb_resize_vectors.json"
"$repository_root/reference/run_msgb_vector.sh" --adapters > "$msgb_output_dir/msgb_adapters_vectors.json"

prim_output_dir="$output_dir/prim"
mkdir -p "$prim_output_dir"
"$repository_root/reference/run_prim_vector.sh" > "$prim_output_dir/prim_vectors.json"

crc_output_dir="$output_dir/crc"
mkdir -p "$crc_output_dir"
"$repository_root/reference/run_crc_vector.sh" byte > "$crc_output_dir/crc_byte_vectors.json"
"$repository_root/reference/run_crc_vector.sh" generic > "$crc_output_dir/crc_generic_vectors.json"

conv_output_dir="$output_dir/conv"
mkdir -p "$conv_output_dir"
conv_fresh=$(mktemp "$conv_output_dir/.conv_vectors.XXXXXX")
trap 'rm -f "$conv_fresh"' EXIT
trap 'exit 1' HUP INT TERM
"$repository_root/reference/run_conv_vector.sh" all > "$conv_fresh"
mv "$conv_fresh" "$conv_output_dir/conv_vectors.ndjson"

# Time evidence builds exact sources only in temporary storage. Publish atomically.
time_output_dir="$output_dir/time"
mkdir -p "$time_output_dir"
time_fresh=$(mktemp "$time_output_dir/.time_vectors.XXXXXX")
trap 'rm -f "$time_fresh"' EXIT
"$repository_root/reference/run_time_vector.sh" all > "$time_fresh"
mv "$time_fresh" "$time_output_dir/time_vectors.ndjson"

# TDef golden observations and separate sanitizer diagnostics. Generate both
# before publishing either, preserving existing fixtures if a probe fails.
tdef_output_dir="$output_dir/tdef"
mkdir -p "$tdef_output_dir"
tdef_fresh=$(mktemp "$tdef_output_dir/.tdef_vectors.XXXXXX")
tdef_diag_fresh=$(mktemp "$tdef_output_dir/.tdef_diagnostics.XXXXXX")
trap 'rm -f "$tdef_fresh" "$tdef_diag_fresh"' EXIT
"$repository_root/reference/run_tdef_vector.sh" all > "$tdef_fresh"
"$repository_root/reference/run_tdef_vector.sh" --diagnostics > "$tdef_diag_fresh"
mv "$tdef_fresh" "$tdef_output_dir/tdef_vectors.ndjson"
mv "$tdef_diag_fresh" "$tdef_output_dir/tdef_diagnostics.json"

# Stage 4 time_cc: successful traces and isolated sanitizer/assertion diagnostics.
# Stage both outputs before publishing; manifests are separately content-pinned.
time_cc_output_dir="$output_dir/time_cc"
mkdir -p "$time_cc_output_dir"
time_cc_fresh=$(mktemp "$time_cc_output_dir/.time_cc_vectors.XXXXXX")
time_cc_diag_fresh=$(mktemp "$time_cc_output_dir/.time_cc_diagnostics.XXXXXX")
trap 'rm -f "$time_cc_fresh" "$time_cc_diag_fresh"' EXIT
"$repository_root/reference/run_time_cc_vector.sh" all > "$time_cc_fresh"
"$repository_root/reference/run_time_cc_vector.sh" --diagnostics > "$time_cc_diag_fresh"
mv "$time_cc_fresh" "$time_cc_output_dir/time_cc_vectors.ndjson"
mv "$time_cc_diag_fresh" "$time_cc_output_dir/time_cc_diagnostics.json"

# Stage 5a standalone rate-counter calls and isolated null diagnostics.
rate_ctr_output_dir="$output_dir/rate_ctr"
mkdir -p "$rate_ctr_output_dir"
rate_ctr_fresh=$(mktemp "$rate_ctr_output_dir/.rate_ctr_vectors.XXXXXX")
rate_ctr_diag_fresh=$(mktemp "$rate_ctr_output_dir/.rate_ctr_diagnostics.XXXXXX")
trap 'rm -f "$rate_ctr_fresh" "$rate_ctr_diag_fresh"' EXIT
"$repository_root/reference/run_rate_ctr_vector.sh" all > "$rate_ctr_fresh"
"$repository_root/reference/run_rate_ctr_vector.sh" --diagnostics > "$rate_ctr_diag_fresh"
mv "$rate_ctr_fresh" "$rate_ctr_output_dir/rate_ctr_vectors.tsv"
mv "$rate_ctr_diag_fresh" "$rate_ctr_output_dir/rate_ctr_diagnostics.json"

# Stage 6: each FSM evidence family was separately gated before implementation.
for fsm_family in fsm_core fsm_timer fsm_tree fsm_registry fsm_shared; do
    fsm_output_dir="$output_dir/$fsm_family"
    mkdir -p "$fsm_output_dir"
    fsm_fresh=$(mktemp "$fsm_output_dir/.${fsm_family}_vectors.XXXXXX")
    fsm_diag_fresh=$(mktemp "$fsm_output_dir/.${fsm_family}_diagnostics.XXXXXX")
    trap 'rm -f "$fsm_fresh" "$fsm_diag_fresh"' EXIT
    "$repository_root/reference/run_${fsm_family}_vector.sh" all > "$fsm_fresh"
    "$repository_root/reference/run_${fsm_family}_vector.sh" --diagnostics > "$fsm_diag_fresh"
    cp "$repository_root/reference/${fsm_family}_provenance.json" "$fsm_output_dir/"
    cp "$repository_root/reference/${fsm_family}_generator_provenance.json" "$fsm_output_dir/"
    mv "$fsm_fresh" "$fsm_output_dir/${fsm_family}_vectors.tsv"
    mv "$fsm_diag_fresh" "$fsm_output_dir/${fsm_family}_diagnostics.json"
done

# Stage 7 selected rate-counter groups/rollups; native evidence stays separate.
rate_ctr_group_output_dir="$output_dir/rate_ctr_group"
mkdir -p "$rate_ctr_group_output_dir"
rate_ctr_group_fresh=$(mktemp "$rate_ctr_group_output_dir/.vectors.XXXXXX")
rate_ctr_group_diag_fresh=$(mktemp "$rate_ctr_group_output_dir/.diagnostics.XXXXXX")
rate_ctr_group_native_fresh=$(mktemp "$rate_ctr_group_output_dir/.native.XXXXXX")
trap 'rm -f "$rate_ctr_group_fresh" "$rate_ctr_group_diag_fresh" "$rate_ctr_group_native_fresh"' EXIT
"$repository_root/reference/run_rate_ctr_group_vector.sh" all > "$rate_ctr_group_fresh"
"$repository_root/reference/run_rate_ctr_group_vector.sh" --diagnostics > "$rate_ctr_group_diag_fresh"
"$repository_root/reference/run_rate_ctr_group_vector.sh" --native > "$rate_ctr_group_native_fresh"
cp "$repository_root/reference/rate_ctr_group_provenance.json" "$rate_ctr_group_output_dir/"
cp "$repository_root/reference/rate_ctr_group_generator_provenance.json" "$rate_ctr_group_output_dir/"
mv "$rate_ctr_group_fresh" "$rate_ctr_group_output_dir/rate_ctr_group_vectors.tsv"
mv "$rate_ctr_group_diag_fresh" "$rate_ctr_group_output_dir/rate_ctr_group_diagnostics.json"
mv "$rate_ctr_group_native_fresh" "$rate_ctr_group_output_dir/rate_ctr_group_native.tsv"
