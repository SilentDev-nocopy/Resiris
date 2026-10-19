# Resiris full file integration

Installs:
- Linux `*.resy` MIME type
- Linux file-manager icon `application-x-resy`
- Seti-based VS Code theme where only `*.resy` files use the RESY icon

Run `./install.sh` to install everything.
Run `./uninstall.sh` to remove everything.

The Linux icon preserves the approved artwork in a high-resolution SVG
container. The VS Code theme is generated from the locally installed Seti
theme, so the built-in Seti installation is not modified.

Resiris is terminal-only; no desktop application entry is installed.
