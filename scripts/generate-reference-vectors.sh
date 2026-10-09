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
