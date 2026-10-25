#!/bin/sh
# Removes the optional osmocore-rs path dependency from Cargo.toml and
# Cargo.lock in place, for CI and Docker builds that cannot see
# ../libosmocore-rs. The `osmocore` feature stays declared but no longer builds,
# so do not enable it afterwards. Fails if pruning would add or change any
# locked package, so later `--locked` builds still use the committed versions.
set -eu

cd "$(dirname "$0")/.."

grep -q '^osmocore-rs = ' Cargo.toml || {
    echo "without-osmocore: no osmocore-rs dependency in Cargo.toml" >&2
    exit 1
}

sed -i \
    -e '/^osmocore-rs = /d' \
    -e '/^osmocore = /s/"dep:osmocore-rs", *//' \
    Cargo.toml

cp Cargo.lock Cargo.lock.orig
trap 'rm -f Cargo.lock.orig' EXIT
cargo metadata --offline --format-version 1 > /dev/null 2>&1 \
    || cargo metadata --format-version 1 > /dev/null

if diff Cargo.lock.orig Cargo.lock | grep -q '^>'; then
    echo "without-osmocore: pruning changed locked packages:" >&2
    diff Cargo.lock.orig Cargo.lock >&2 || true
    exit 1
fi
if grep -q '^name = "osmocore-rs"$' Cargo.lock; then
    echo "without-osmocore: osmocore-rs is still locked" >&2
    exit 1
fi
echo "without-osmocore: removed osmocore-rs from Cargo.toml and Cargo.lock"
