# Resiris

> Resiris is the programming language designed for [Galena](https://github.com/SilentDev-nocopy/Galena.git), a standalone programmable computing environment.

This repository provides the PC-side development environment: a tokenizer, parser, AST and interpreter for `.resy` source files, along with optional editor and shell integration.

For now, only the Python version aka. "PyResy" is available!

Resiris is a separate project from Galena.
---

## Requirements

- Python 3

Optional, for `.resy` file integration:

- VS Code / Code - OSS / VSCodium
- `xdg-mime`
- `update-mime-database`

---

## Running a program

Download or clone this repository, then open a terminal in the project folder.

Run a specific Resiris program with:

```bash
python3 -m resiris program.resy
```

## Installing `resiris` as a terminal command

To start Resiris by simply typing `resiris` from any terminal, run the installer from inside the project folder:

```bash
bash ./install_resiris.sh
```

This installer:

- creates a `resiris` symlink in `~/.local/bin`
- installs the `.resy` Linux file-manager integration

If a VS Code-compatible CLI is available, the installer will also ask whether to install the **Resiris Language Support** extension. Pass `--yes` to skip the prompt and install it in one step:

```bash
bash ./install_resiris.sh --yes
```

The extension is **not** installed automatically otherwise — you can decline it during setup, or install it separately later directly from VS Code.

The VS Code extension adds:

- `.resy` syntax highlighting, indentation settings and snippets
- a custom `.resy` file icon via the Resiris Seti icon theme

Installing the terminal tool alone does not install the VS Code extension or its icon theme, and vice versa.

Once installed, `resiris program.resy` and plain `resiris` behave exactly as described above — there is no automatic `main.resy` execution.

---

## Error output

Resiris errors are printed as concise CLI errors without exposing the internal Python traceback. Source errors from the tokenizer, parser or runtime include the Resiris error type and its message, for example:

```text
UnknownVariableError: line 1, column 1: x: unknown name
```

Invalid command-line arguments are reported separately as `Resiris CLI error:` messages instead of printing the full argparse usage page.

---

## Uninstalling

To remove the installed terminal command and `.resy` Linux integration, run:

```bash
bash ./uninstall_resiris.sh
```

The project files themselves are not deleted.

---

## Related projects

### Galena

The standalone programmable computing environment that Resiris programs are compiled and run for.
