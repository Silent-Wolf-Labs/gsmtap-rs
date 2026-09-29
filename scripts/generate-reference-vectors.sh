#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
output_dir="$repository_root/tests/vectors"

: "${LIBOSMOCORE_ROOT:=/mnt/storage/git/libosmocore/_install}"
export LIBOSMOCORE_ROOT
export PKG_CONFIG_PATH="$LIBOSMOCORE_ROOT/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$LIBOSMOCORE_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

mkdir -p "$output_dir"
for case_name in $("$repository_root/reference/run_gsmtap_makemsg_ex_vector.sh" --list); do
	"$repository_root/reference/run_gsmtap_makemsg_ex_vector.sh" --case "$case_name" \
		> "$output_dir/$case_name.json"
done

bcd_output_dir="$output_dir/bcd"
mkdir -p "$bcd_output_dir"
"$repository_root/reference/run_bcd_conversion_vector.sh" \
	> "$bcd_output_dir/bcd_conversion_vectors.json"
