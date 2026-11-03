# Resiris C++ port

C++17 port of the Python prototype in `../PyResy`. The differential test script
compares the output of its fixture programs with the Python implementation.

All commands below run from this directory (`CppResy/`).

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

After a source change, just `cmake --build build -j`.

## Install

```sh
cmake --install build --prefix ~/.local
```

This puts the `resiris` command in `~/.local/bin/resiris`, which works from any
directory:

```sh
resiris examples/01_basics.resy
```

If you later `pip install` the Python package, it installs a `resiris` command
too and overwrites this one. Keep them in separate prefixes, or use the build
output directly as `./build/resiris`.

## Uninstall

```sh
tools/uninstall.sh            # remove the installed command
tools/uninstall.sh --purge    # also delete build/ and ~/.config/resiris
```

Removes exactly the files `cmake --install` wrote, nothing else. Safe to run
twice. After `--purge` you have to build again from scratch.

## Run

```sh
./build/resiris examples/01_basics.resy            # math, types, conversions
./build/resiris examples/02_control_flow.resy      # if/elif/else, mat, recursion
./build/resiris examples/03_functions.resy         # functions, FunctionalObject
./build/resiris examples/04_timer_lifecycle.resy   # RSBase timer, Ctrl+C to stop
```

Also: `--version`, `--help`, `--enable-frontend-visibility`, and
`--disable-frontend-visibility`. Frontend visibility adds `<TOKENS>` and `<AST>`
output after a program completes normally; its setting is stored in
`~/.config/resiris/frontend_visibility`.

## Test

```sh
./build/resiris-tests                  # currently 140 Catch2 cases
./tools/differential_test.sh           # 29 programs, compared with the prototype
python3 tools/audit_sweep.py           # 589 programs taken from the Python test suite
```

Both scripts run the Python prototype themselves, so they work from this
directory without any setup. Pass a file to check just one program:

```sh
./tools/differential_test.sh my_program.resy
```

## Compare a program with the Python prototype

The Python side has to run from `../PyResy`, the C++ side from anywhere:

```sh
cd ../PyResy && python3 -m resiris.cli ~/prog.resy > /tmp/py.txt 2>&1
cd ../CppResy && ./build/resiris ~/prog.resy > /tmp/cpp.txt 2>&1
diff /tmp/py.txt /tmp/cpp.txt && echo identical
```

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

## Notes

- `int` is `int64_t`, not Python's arbitrary-precision integer.
- The CLI registers `RSMath`, `RSBase`, `RSConvert`, and `RSSignal` as compiled-in
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
|---|---|
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
