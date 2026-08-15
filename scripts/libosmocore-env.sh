#!/usr/bin/env sh
# Source this file before building or running a live libosmocore reference
# harness. LIBOSMOCORE_ROOT may be overridden for another checked-out build.

: "${LIBOSMOCORE_ROOT:=/mnt/storage/git/libosmocore/_install}"

export LIBOSMOCORE_ROOT
export PKG_CONFIG_PATH="$LIBOSMOCORE_ROOT/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$LIBOSMOCORE_ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

printf 'Using libosmocore: %s\n' "$LIBOSMOCORE_ROOT"
pkg-config --modversion libosmocore
