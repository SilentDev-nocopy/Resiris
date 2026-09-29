# Resiris C++ port

C++17 port of the Python prototype in `../PyResy`. Behaviour is identical: error
messages, float formatting and the token/AST dump match byte for byte.

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

Also: `--version`, `--help`, `--enable-frontend-visibility` (adds `<TOKENS>` and
`<AST>` output).

## Test

```sh
./build/resiris-tests                  # 114 Catch2 cases
./tools/differential_test.sh           # 29 programs, compared with the prototype
python3 tools/audit_sweep.py           # 589 programs taken from the Python tests
```

Both scripts run the Python prototype themselves, so they work from this
directory without any setup. Pass a file to check just one program:

```sh
./tools/differential_test.sh my_program.resy
```

## Compare a program with the prototype

The Python side has to run from `../PyResy`, the C++ side from anywhere:

```sh
cd ../PyResy && python3 -m resiris.cli ~/prog.resy > /tmp/py.txt 2>&1
cd ../CppResy && ./build/resiris ~/prog.resy > /tmp/cpp.txt 2>&1
diff /tmp/py.txt /tmp/cpp.txt && echo identical
```

## Layout

| Path | Contents |
| --- | --- |
| `include/resiris/`, `src/` | tokenizer, parser, interpreter, modules, CLI |
| `include/resiris/rsmath.hpp`, `include/resiris/rsbase.hpp` | the two built-in modules |
| `examples/` | runnable example programs |
| `tests/` | Catch2 suite and the differential fixtures |
| `tools/` | `differential_test.sh`, `audit_sweep.py`, `uninstall.sh` |
| `third_party/catch.hpp` | Catch2 2.13.10 |

## Notes

- `int` is `int64_t`, not Python's arbitrary precision integer.
- Modules are compiled in, not loaded from files at runtime, so
  `<include> SomeUserModule` only works in the Python prototype. This is the
  intended direction for the embedded target.
- Adding a module: implement the five methods of `Module`
  (`include/resiris/module.hpp`), add the source to `CMakeLists.txt`, and add
  one line to the registration list in `src/cli.cpp`.
