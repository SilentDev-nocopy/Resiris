# Resiris C++ port

C++17 port of the Resiris language: tokenizer, parser, interpreter, module
system, the `RSMath` and `RSBase` modules, and the CLI. The Python prototype in
`../PyResy` is the behavioural specification - error messages, float formatting
and every diagnostic are meant to match it byte for byte.

## Install the `resiris` command

The Python package exposes a `resiris` console script
(`[project.scripts] resiris = "resiris.cli:main"`). This port installs under the
same name, so the workflow is identical:

```sh
cmake --install build --prefix ~/.local
resiris examples/01_basics.resy
```

The C++ port resolves `RSMath` and `RSBase` internally, so unlike the Python
one it runs from any directory with no `PYTHONPATH`, virtualenv or `cd` setup.

Note: if you later `pip install` the Python package it will overwrite this
command in the same `bin` directory. Keep them in separate prefixes, or invoke
the build output directly as `./build/resiris`.

## Try it

Ready made programs live in `examples/`:

```sh
./build/resiris examples/01_basics.resy            # math, types, conversions
./build/resiris examples/02_control_flow.resy      # if/elif/else, mat, recursion
./build/resiris examples/03_functions.resy         # functions, FunctionalObject
./build/resiris examples/04_timer_lifecycle.resy   # RSBase timer + PROCESS, Ctrl+C to stop
```

`04_timer_lifecycle.resy` runs the PROCESS lifecycle at 10 FPS until you press
Ctrl+C, which stops it the same way the Python CLI does.

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

Targets:

- `resiris` - the library (tokenizer, parser, interpreter, modules)
- `resiris` (from `resiris-cli`) - the command line runner
- `resiris-tests` - the Catch2 test suite

## Test

```sh
./build/resiris-tests          # or: ctest --test-dir build
```

## Differential test against the prototype

`tools/differential_test.sh` runs every `tests/differential/*.resy` fixture
through both the Python prototype and this port, then compares stdout, stderr
and the exit code. A C++/Python mismatch in any user visible behaviour fails
the run.

```sh
cmake --build build -j
./tools/differential_test.sh
```

## Run a program

```sh
./build/resiris path/to/program.resy
./build/resiris --enable-frontend-visibility program.resy   # + <TOKENS>/<AST>
```

Both CLIs read the same frontend visibility setting from
`~/.config/resiris/frontend_visibility`, so the dumped token stream and AST are
compared as well when it is enabled.

## Layout

| Path | Contents |
| --- | --- |
| `include/resiris/value.hpp` | runtime `Value`, `ModuleObject`, `FunctionalObject`, `FrameAwareState` |
| `include/resiris/errors.hpp` | error hierarchy mirroring the prototype exceptions |
| `include/resiris/token.hpp`, `src/tokenizer.cpp` | token types and the scanner |
| `include/resiris/ast.hpp`, `src/parser.cpp` | AST nodes and the recursive descent parser |
| `include/resiris/interpreter.hpp`, `src/interpreter.cpp` | scopes, statements, expressions, lifecycles |
| `include/resiris/module.hpp` | `Module` interface and `ModuleRegistry` (`<include>` handling) |
| `include/resiris/rsmath.hpp`, `src/rsmath.cpp` | the `RSMath` module |
| `include/resiris/rsbase.hpp`, `src/rsbase.cpp` | the `RSBase` timer module |
| `src/cli.cpp` | CLI, banner, frontend visibility, AST dump |
| `tests/` | Catch2 suite (`test_*.cpp`) and the differential fixtures |
| `third_party/catch.hpp` | Catch2 2.13.10 single header |

## Behaviour notes

Differences from the prototype that are deliberate:

- `int` is `int64_t` instead of Python's arbitrary precision integer.
- A module domain error (`RSMath.ln(0)`) surfaces as a `ModuleError` with the
  prototype's message; Python lets the raw `ValueError` escape.
