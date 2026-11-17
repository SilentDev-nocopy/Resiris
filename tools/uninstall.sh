#!/usr/bin/env bash
# Uninstalls the Resiris CLI: removes exactly the files that
# `cmake --install` recorded in the install manifest.
#
# Usage: tools/uninstall.sh [--prefix ~/.local] [--purge]
#
#   --prefix   the prefix the tool was installed to (default: ~/.local)
#   --purge    also delete the build/ directory and local settings
#
# Only paths that are listed in the manifest AND inside the prefix are
# removed, so a stale manifest can never delete anything else.

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(dirname "$script_dir")"
manifest="$project_dir/build/install_manifest.txt"
prefix="$HOME/.local"
purge=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix) prefix="$2"; shift 2 ;;
        --purge) purge=1; shift ;;
        -h|--help)
            sed -n '2,14p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

prefix="$(cd "$prefix" 2>/dev/null && pwd || echo "$prefix")"

if [ ! -f "$manifest" ]; then
    echo "no install manifest at $manifest - nothing to uninstall."
    echo "(the CLI was never installed from this build directory)"
    exit 0
fi

removed=0
kept=0

while IFS= read -r path || [ -n "$path" ]; do
    [ -n "$path" ] || continue

    # Safety: never touch anything outside the prefix.
    case "$path" in
        "$prefix"/*) ;;
        *)
            echo "  skip (outside prefix): $path"
            kept=$((kept + 1))
            continue
            ;;
    esac

    if [ -e "$path" ] || [ -L "$path" ]; then
        rm -f "$path"
        echo "  removed: $path"
        removed=$((removed + 1))
    else
        echo "  already gone: $path"
    fi
done < "$manifest"

# Drop the manifest line so a second run is a no-op.
: > "$manifest"

# Remove the bin directory only if we emptied it. Never the prefix itself: it
# may be a directory the user created and still cares about.
if [ -d "$prefix/bin" ] && [ -z "$(ls -A "$prefix/bin" 2>/dev/null)" ]; then
    rmdir "$prefix/bin" 2>/dev/null && echo "  removed empty directory: $prefix/bin"
fi

echo
echo "uninstalled: $removed file(s) removed, $kept skipped"

if [ "$purge" -eq 1 ]; then
    rm -rf "$project_dir/build"
    echo "  removed: $project_dir/build"
    settings="$HOME/.config/resiris"
    if [ -d "$settings" ]; then
        rm -rf "$settings"
        echo "  removed: $settings"
    fi
    echo "purge done: build directory and user settings deleted"
else
    echo "kept: $project_dir/build (source and build output)"
    echo "note: --purge also deletes build/ and ~/.config/resiris"
fi
