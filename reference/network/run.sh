#!/usr/bin/env sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
reference_dir=$(CDPATH= cd -- "$script_dir/../../../libosmocore-rs/libosmocore" && pwd)
python3 - "$script_dir" "$reference_dir" <<'PY'
import hashlib,json,pathlib,sys
script,root=map(pathlib.Path,sys.argv[1:])
provenance=json.loads((script/'provenance.json').read_text())
if provenance['revision'] != '950430e829a3dc1d162aa241bc0505745c5a7311': raise SystemExit('Reference revision mismatch')
for path,expected in provenance['files'].items():
    if hashlib.sha256((root/path).read_bytes()).hexdigest()!=expected: raise SystemExit('Reference mismatch: '+path)
generator=json.loads((script/'generator_provenance.json').read_text())
for path,expected in generator.items():
    if hashlib.sha256((script/path).read_bytes()).hexdigest()!=expected: raise SystemExit('Generator mismatch: '+path)
PY
make -C "$reference_dir/src/core" >/dev/null
library="$reference_dir/src/core/.libs/libosmocore.so"
test -f "$library" || { echo 'Required local C library missing' >&2; exit 1; }
binary=$(mktemp "${TMPDIR:-/tmp}/network-vector.XXXXXX")
trap 'rm -f "$binary"' EXIT
trap 'exit 1' HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -isystem "$reference_dir/include" \
    "$script_dir/network_vector.c" "$library" -ltalloc \
    -Wl,-rpath,"$reference_dir/src/core/.libs" -o "$binary"
LD_LIBRARY_PATH="$reference_dir/src/core/.libs" LD_PRELOAD= LD_AUDIT= "$binary"
