#!/usr/bin/env bash
# Source this file in Bash before building against the canonical C installation.
# Resolve paths from this script so sourcing works from any working directory.

_gsmtap_reference_env() {
    local script_dir reference_dir install_dir requested_dir
    script_dir=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd) || return 1
    reference_dir=$(CDPATH= cd -- "$script_dir/../../libosmocore-rs/libosmocore" && pwd) || return 1
    install_dir=$(CDPATH= cd -- "$reference_dir/_install" && pwd) || return 1
    requested_dir=$(CDPATH= cd -- "${LIBOSMOCORE_ROOT:-$install_dir}" && pwd) || return 1
    if [ "$requested_dir" != "$install_dir" ]; then
        printf 'Reference must use the canonical local installation: %s\n' "$install_dir" >&2
        return 1
    fi
    if [ ! -f "$install_dir/lib/pkgconfig/libosmocore.pc" ]; then
        printf 'Missing canonical libosmocore installation: %s\n' "$install_dir" >&2
        return 1
    fi

    export LIBOSMOCORE_ROOT="$install_dir"
    export PKG_CONFIG_PATH="$install_dir/lib/pkgconfig"
    export LD_LIBRARY_PATH="$install_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    if [ "$(pkg-config --variable=prefix libosmocore)" != "$install_dir" ]; then
        printf 'pkg-config did not resolve the canonical libosmocore installation\n' >&2
        return 1
    fi
    printf 'Using libosmocore: %s\n' "$LIBOSMOCORE_ROOT"
    pkg-config --modversion libosmocore
}

if _gsmtap_reference_env; then
    unset -f _gsmtap_reference_env
else
    unset -f _gsmtap_reference_env
    return 1 2>/dev/null || exit 1
fi
