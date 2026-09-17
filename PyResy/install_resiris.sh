#!/usr/bin/env bash
#
# Resiris installer for Linux (any distro).
#
# Installs:
#   - the `resiris` command (symlink into a bin dir on PATH)
#   - bash and zsh completion for `resiris` and .resy files
#   - MIME / file-manager integration for .resy files
#   - the Resiris Language Support VS Code extension (optional)
#
# Safe to run repeatedly (idempotent).
#
# Usage:
#   install_resiris.sh [--yes]
#
#   --yes   install the VS Code extension without asking

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

INSTALL_VSIX=0
for arg in "$@"; do
    case "$arg" in
        --yes)
            INSTALL_VSIX=1
            ;;
        --help|-h)
            echo "Usage: install_resiris.sh [--yes]"
            echo
            echo "  --yes   install the VS Code extension without asking"
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg (see --help)" >&2
            exit 2
            ;;
    esac
done

# ---------------------------------------------------------------------------
# Directories (XDG-aware, so this works on any Linux)
# ---------------------------------------------------------------------------

DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
BIN_DIR="${XDG_BIN_HOME:-$HOME/.local/bin}"
MIME_DIR="$DATA_HOME/mime/packages"
ICONS_DIR="$DATA_HOME/icons/hicolor/scalable/mimetypes"
BASH_COMPLETION_DIR="$DATA_HOME/bash-completion/completions"
ZSH_COMPLETION_DIR="$DATA_HOME/zsh/site-functions"

# ---------------------------------------------------------------------------
# Python detection (honours $RESIRIS_PYTHON, otherwise python3 on PATH)
# ---------------------------------------------------------------------------

PYTHON="${RESIRIS_PYTHON:-}"
if [[ -z "$PYTHON" ]]; then
    PYTHON="$(command -v python3 || true)"
fi

if [[ -z "$PYTHON" ]]; then
    echo "ERROR: Python 3 was not found on PATH." >&2
    echo "Install Python 3 (>= 3.10), then run this installer again." >&2
    exit 1
fi

if ! "$PYTHON" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)' >/dev/null 2>&1; then
    echo "ERROR: Resiris requires Python 3.10 or newer." >&2
    echo "Found: $("$PYTHON" --version 2>&1 || true)" >&2
    exit 1
fi

echo "Using Python: $("$PYTHON" --version 2>&1)"

# ---------------------------------------------------------------------------
# Terminal command
# ---------------------------------------------------------------------------

mkdir -p "$BIN_DIR"
ln -sfn "$ROOT/bin/resiris" "$BIN_DIR/resiris"

if [[ "$(readlink -f "$BIN_DIR/resiris")" != "$(readlink -f "$ROOT/bin/resiris")" ]]; then
    echo "ERROR: Resiris CLI installation points to the wrong executable." >&2
    exit 1
fi

echo "Installed: $BIN_DIR/resiris"

case ":$PATH:" in
    *":$BIN_DIR:"*) ;;
    *)
        echo "WARNING: $BIN_DIR is not on your PATH."
        echo "         Add it to your shell configuration, for example:"
        echo "         export PATH=\"\$HOME/.local/bin:\$PATH\""
        ;;
esac

# ---------------------------------------------------------------------------
# Shell completion
# ---------------------------------------------------------------------------

mkdir -p "$BASH_COMPLETION_DIR" "$ZSH_COMPLETION_DIR"

if [[ -f "$ROOT/pre_packaging/shell/resiris.bash" ]]; then
    cp -f "$ROOT/pre_packaging/shell/resiris.bash" "$BASH_COMPLETION_DIR/resiris"
    echo "Installed: bash completion"
fi

if [[ -f "$ROOT/pre_packaging/shell/_resiris" ]]; then
    cp -f "$ROOT/pre_packaging/shell/_resiris" "$ZSH_COMPLETION_DIR/_resiris"
    echo "Installed: zsh completion"
fi

# ---------------------------------------------------------------------------
# MIME integration (file manager icons for .resy files)
# ---------------------------------------------------------------------------

mkdir -p "$MIME_DIR" "$ICONS_DIR"

if [[ -f "$ROOT/pre_packaging/linux/mime/resy.xml" ]]; then
    cp -f "$ROOT/pre_packaging/linux/mime/resy.xml" "$MIME_DIR/resy.xml"

    if [[ -f "$ROOT/pre_packaging/linux/icons/application-x-resy.svg" ]]; then
        cp -f "$ROOT/pre_packaging/linux/icons/application-x-resy.svg" \
            "$ICONS_DIR/application-x-resy.svg"
    fi

    if command -v update-mime-database >/dev/null 2>&1; then
        update-mime-database "$DATA_HOME/mime" >/dev/null 2>&1 || true
        echo "Installed: .resy MIME integration"
    else
        echo "WARNING: update-mime-database was not found."
        echo "         .resy file icons may require a re-login to appear."
    fi
fi

# Assign a default editor to .resy files when a VS Code desktop entry exists.
desktop_dirs=(
    "$HOME/.local/share/applications"
    "$HOME/.local/share/flatpak/exports/share/applications"
    "/var/lib/flatpak/exports/share/applications"
    "/usr/share/applications"
)
desktop_candidates=(
    code.desktop
    visual-studio-code.desktop
    code-insiders.desktop
    codium.desktop
    vscodium.desktop
)

CODE_DESKTOP=""
for desktop in "${desktop_candidates[@]}"; do
    for dir in "${desktop_dirs[@]}"; do
        if [[ -f "$dir/$desktop" ]]; then
            CODE_DESKTOP="$desktop"
            break 2
        fi
    done
done

if command -v xdg-mime >/dev/null 2>&1; then
    if [[ -n "$CODE_DESKTOP" ]]; then
        xdg-mime default "$CODE_DESKTOP" application/x-resy >/dev/null 2>&1 || true
        echo "Default editor for .resy files: $CODE_DESKTOP"
    else
        echo "WARNING: No VS Code desktop entry was found; .resy files"
        echo "         were not assigned a default editor."
    fi
else
    echo "WARNING: xdg-mime was not found; .resy files were not assigned"
    echo "         a default editor."
fi

# ---------------------------------------------------------------------------
# VS Code CLI detection (PATH first, then common locations)
# ---------------------------------------------------------------------------

CODE_CMD=""
for cmd in code code-insiders code-oss codium; do
    if command -v "$cmd" >/dev/null 2>&1; then
        CODE_CMD="$(command -v "$cmd")"
        break
    fi
done

if [[ -z "$CODE_CMD" ]]; then
    for candidate in \
        /usr/bin/code /usr/bin/code-insiders /usr/bin/code-oss /usr/bin/codium \
        /usr/local/bin/code /usr/local/bin/codium \
        /snap/bin/code /snap/bin/codium \
        "$HOME/.local/bin/code" "$HOME/.local/bin/codium"
    do
        if [[ -x "$candidate" ]]; then
            CODE_CMD="$candidate"
            break
        fi
    done
fi

# ---------------------------------------------------------------------------
# Resiris VS Code extension (built only when necessary)
# ---------------------------------------------------------------------------

find_latest_vsix() {
    local dir="$1"
    local found=""
    if [[ -d "$dir" ]]; then
        found="$(find "$dir" -maxdepth 1 -type f \
            -name 'resiris-language-support-*.vsix' -print0 2>/dev/null \
            | tr '\0' '\n' | sort -V | tail -n 1)"
    fi
    printf '%s' "$found"
}

VSIX_DIR="$ROOT/pre_packaging/vscode/build"
VSIX="$(find_latest_vsix "$VSIX_DIR")"

if [[ -z "$VSIX" && -f "$ROOT/pre_packaging/vscode/build_vsix.py" ]]; then
    echo "Building the Resiris VS Code extension..."
    if "$PYTHON" "$ROOT/pre_packaging/vscode/build_vsix.py" >/dev/null 2>&1; then
        VSIX="$(find_latest_vsix "$VSIX_DIR")"
    else
        echo "WARNING: Could not build the VS Code extension." >&2
    fi
fi

echo

INSTALL_EXTENSION=0
if [[ -z "$CODE_CMD" ]]; then
    echo "No VS Code-compatible CLI was found."
    echo "The Resiris VS Code extension was not installed."
    echo "Install VS Code, then re-run: install_resiris.sh"
elif [[ -z "$VSIX" ]]; then
    echo "WARNING: The VS Code extension package is not available."
    echo "         Resiris terminal/Linux integration was still installed."
elif [[ "$INSTALL_VSIX" -eq 1 ]]; then
    INSTALL_EXTENSION=1
else
    printf 'Install Resiris Language Support for VS Code (includes .resy icons)? [y/N] '
    answer=""
    read -r answer || true
    if [[ "${answer:-}" =~ ^[Yy] ]]; then
        INSTALL_EXTENSION=1
    fi
fi

if [[ "$INSTALL_EXTENSION" -eq 1 ]]; then
    if "$CODE_CMD" --install-extension "$VSIX" --force >/dev/null 2>&1; then
        echo "Resiris VS Code extension installed ($(basename "$VSIX"))."
    else
        echo "WARNING: Could not install the VS Code extension." >&2
    fi
else
    echo "VS Code extension not installed."
    echo "Re-run the installer and answer yes, or install it from VS Code later."
fi

# ---------------------------------------------------------------------------
# Verification
# ---------------------------------------------------------------------------

echo
if [[ -x "$BIN_DIR/resiris" ]] && "$BIN_DIR/resiris" --version >/dev/null 2>&1; then
    echo "Resiris installed."
    echo
    echo "Run:"
    echo "  resiris <file.resy>"
    echo
else
    echo "WARNING: The resiris command could not be verified." >&2
    exit 1
fi