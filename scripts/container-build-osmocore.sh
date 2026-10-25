#!/bin/sh
# Builds the workbench image with the `osmocore` feature. Stages the packaged
# files of ../libosmocore-rs/osmocore-rs in a temporary directory and passes it
# as the `osmocore-rs` build context. Extra arguments go to `buildah build`, for
# example `--tag gsmtap-workbench:osmocore`.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
crate=$root/../libosmocore-rs/osmocore-rs
if [ ! -f "$crate/Cargo.toml" ]; then
    echo "container-build-osmocore: $crate not found" >&2
    exit 1
fi

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cp -R "$crate/Cargo.toml" "$crate/README.md" "$crate/LICENSE" "$crate/src" "$stage/"

buildah build \
    --build-arg FEATURES=osmocore \
    --build-context osmocore-rs="$stage" \
    --target workbench \
    "$@" \
    "$root"
