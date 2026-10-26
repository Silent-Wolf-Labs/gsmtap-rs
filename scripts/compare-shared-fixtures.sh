#!/usr/bin/env sh
# Compare the fixtures, readers and manifests this repository shares with
# osmocore-rs. Read-only: reports every differing or missing copy and exits
# nonzero if there is one.
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
osmocore_tests="${OSMOCORE_RS_DIR:-$repository_root/../libosmocore-rs/osmocore-rs}/tests"

if [ ! -d "$osmocore_tests/compatibility/vectors" ]; then
	echo "osmocore-rs tests not found at $osmocore_tests (set OSMOCORE_RS_DIR)" >&2
	exit 2
fi

cd "$repository_root"
failures=0
checked=0

compare() {
	checked=$((checked + 1))
	if [ ! -e "$2" ]; then
		echo "missing: $2"
		failures=$((failures + 1))
	elif ! cmp -s "$1" "$2"; then
		echo "differs: $1 $2"
		failures=$((failures + 1))
	fi
}

# Fixtures keep their subdirectory in osmocore-rs, except older families and
# the GSMTAP packet vectors, which sit flat in its vectors directory.
for fixture in $(find tests/vectors -type f | sort); do
	relative=${fixture#tests/vectors/}
	copy="$osmocore_tests/compatibility/vectors/$relative"
	[ -e "$copy" ] || copy="$osmocore_tests/compatibility/vectors/$(basename "$fixture")"
	compare "$fixture" "$copy"
done

for reader in tests/support/*_fixture.rs; do
	compare "$reader" "$osmocore_tests/support/$(basename "$reader")"
done

# Manifests copied next to the fixtures must match the ones the runners pin.
for manifest in $(find reference -name '*provenance.json' | sort); do
	family=$(basename "$manifest" | sed 's/_generator_provenance\.json$//; s/_provenance\.json$//')
	case "$manifest" in
	reference/network/*) copy="tests/vectors/net/$(basename "$manifest")" ;;
	*) copy="tests/vectors/$family/$(basename "$manifest")" ;;
	esac
	compare "$manifest" "$copy"
done

echo "$checked files compared, $failures mismatched"
[ "$failures" -eq 0 ]
