# Resiris

> Resiris is the programming language designed for [Galena](https://github.com/SilentDev-nocopy/Galena.git), a standalone programmable computing environment.

This repository holds the PC-side development environment for `.resy` source files: a tokenizer, parser, AST and interpreter written in C++17, with a `resiris` command-line runner. Resiris is a separate project from Galena.

The original Python prototype ("PyResy") has been moved out of this repository. It is still used as the reference implementation for the differential tools here; see [Related projects](#related-projects).

---

## Requirements

- A C++17 compiler
- CMake 3.16 or newer

Optional, for the differential and audit tools:

- Python 3
- A PyResy checkout next to this directory (`../PyResy`), or the `RESIRIS_PROTOTYPE_DIR` environment variable pointing at one

---

## Build

All commands below run from the project root.

```sh
cmake -S . -B build
cmake --build build -j
```

After a source change, just `cmake --build build -j`.

---

## Run

```sh
./build/resiris examples/01_basics.resy            # math, types, conversions
./build/resiris examples/02_control_flow.resy      # if/elif/else, mat, recursion
./build/resiris examples/03_functions.resy         # functions, FunctionalObject
./build/resiris examples/04_timer_lifecycle.resy   # RSBase timer, Ctrl+C to stop
./build/resiris examples/features.resy             # feature showcase, Ctrl+C to stop
```

Running `resiris` without a file prints usage; there is no implicit `main.resy`.
`resiris --help` shows the options and `resiris --version` prints the version.
`--enable-frontend-visibility` and `--disable-frontend-visibility` toggle the
`<TOKENS>` and `<AST>` output printed after a program completes normally. The
setting is stored in `~/.config/resiris/frontend_visibility`.

Errors are printed as concise CLI messages (for example
`UnknownVariableError: line 1, column 1: x: unknown name`) with no internal
traceback. Invalid command-line arguments are reported as `Resiris CLI error:`
messages.

---

## Install

```sh
cmake --install build --prefix ~/.local
```

This puts the `resiris` command in `~/.local/bin/resiris`, which works from any
directory:

```sh
resiris examples/01_basics.resy
```

If you also install the Python package, its `resiris` command can overwrite this
one. Keep them in separate prefixes, or use the build output directly as
`./build/resiris`.

---

## Uninstall

```sh
tools/uninstall.sh            # remove the installed command
tools/uninstall.sh --purge    # also delete build/ and ~/.config/resiris
```

Only the files recorded in `build/install_manifest.txt` and located inside the
install prefix are removed. The script is safe to run twice; `--purge` deletes
the build directory and local settings, after which you have to build again.

---

## Test

```sh
./build/resiris-tests             # 140 Catch2 cases
./tools/differential_test.sh      # 29 programs, compared with the prototype
python3 tools/audit_sweep.py      # 589 programs taken from the Python test suite
./tools/differential_test.sh my_program.resy   # check just one program
```

The two comparison scripts need a PyResy checkout at `../PyResy` (or set
`RESIRIS_PROTOTYPE_DIR`); they do not need any other setup. The C++ unit tests
run entirely from this repository.

---

## Layout

| Path | Contents |
| --- | --- |
| `include/resiris/`, `src/` | tokenizer, parser, interpreter, CLI |
| `include/resiris/modules/`, `src/modules/` | built-in module interfaces and implementations |
| `examples/` | runnable example programs |
| `tests/` | Catch2 suite and Resiris differential fixtures |
| `txt/info/` | language, module, type, timer and hardware references |
| `txt/ideas/` | design notes; not authoritative API documentation |
| `tools/` | `differential_test.sh`, `audit_sweep.py`, `uninstall.sh` |
| `third_party/catch.hpp` | Catch2 2.13.10 |

---

## Notes

- `int` is `int64_t`, not Python's arbitrary-precision integer.
- The CLI registers `RSMath`, `RSBase`, `RSConvert` and `RSSignal` as compiled-in
  modules. `<include>` selects one of these registered modules; it does not load
  code dynamically.
- The desktop `RSSignal` module is a simulation, not an ESP32 hardware driver.
- Adding a module: implement the six pure virtual methods of `Module`
  (`include/resiris/module.hpp`), add its source to `CMakeLists.txt`, include
  its header, and register it in `src/cli.cpp`.
- The ESP32 component list in `txt/info/hardware_components.txt` is a plan, not
  a record of assembled or integrated hardware.

## Documentation

| File | Scope |
| --- | --- |
| `txt/info/language_reference.txt` | Current syntax, types and execution model |
| `txt/info/type_operations.txt` | `.type()`, `.string()` and `str()` behavior |
| `txt/info/match_statement.txt` | Current `mat` grammar and behavior |
| `txt/info/native_module_system.txt` | Module inclusion, registry and C++ interface |
| `txt/info/rsmath_module.txt` | `RSMath` API |
| `txt/info/rsbase_timer_module.txt` | `RSBase` timer API and frame-clock semantics |
| `txt/info/rsconvert_module.txt` | `RSConvert` API and validation rules |
| `txt/info/rssignal_module.txt` | `RSSignal` desktop simulation and GPIO constants |
| `txt/info/esp32_gpio_table.txt` | ESP32-WROOM-32 pin reference; not the desktop simulator behavior |
| `txt/info/hardware_components.txt` | Proposed calculator hardware, not implemented product features |

---

## Related projects

### Galena

The standalone programmable computing environment that Resiris programs are
compiled and run for.

### PyResy

The Python prototype of Resiris, now maintained as a separate project. It is
the reference implementation used by `tools/differential_test.sh` and
`tools/audit_sweep.py`, and provides the VS Code / file-manager integration.
