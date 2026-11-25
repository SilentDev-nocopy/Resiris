# Resiris

<img src="assets/resiris-file-icon.png" alt="Resiris file icon for .resy files" width="96" align="right">

> Resiris is the programming language designed for [Galena](https://github.com/SilentDev-nocopy/Galena.git), a standalone programmable computing environment.

This repository contains the C++17 tokenizer, parser, AST and interpreter for
`.resy` files, along with the `resiris` CLI. The current version is `1.9-cpp`,
defined in [`include/resiris/version.hpp`](include/resiris/version.hpp). The
Python prototype (PyResy) is a separate project; it is the reference
implementation for the comparison tools below.

## Requirements

- C++17 compiler, CMake 3.16 or newer
- Python 3 and a PyResy checkout for the comparison tools (default `../PyResy`, or set `RESIRIS_PROTOTYPE_DIR`)

## Build

```sh
cmake -S . -B build
cmake --build build -j
```

## Run

```sh
./build/resiris examples/01_basics.resy
```

More programs are in `examples/`; the timer demos run until Ctrl+C. `resiris --help` lists the options, `resiris --version` prints the version, and running without a file prints usage.

## Modules

The CLI registers six built-in modules:

| Module | Purpose |
|---|---|
| `RSBase` | timers and `await` |
| `RSConvert` | text, number and binary conversions |
| `RSMath` | numeric functions |
| `RSNetwork` | ESP32 Wi-Fi and the portable HTML server |
| `RSSignal` | GPIO input and output |
| `RSSystem` | device facts and hardware reports |

`RSNetwork`'s Wi-Fi API and `RSSystem`'s hardware functions are ESP32 only and
raise a `ModuleError` in the CLI; the HTML server binds to `127.0.0.1`. See
[`txt/info/`](txt/info/) for each module's reference.

When writing programs for the ESP32, build longer strings with `+=`: each `+`
recurses in the interpreter, and a long chain can overflow the `loopTask` stack.

## Install / uninstall

```sh
cmake --install build --prefix ~/.local   # installs ~/.local/bin/resiris
tools/uninstall.sh                        # --purge also removes build/ and local settings
```

## Test

```sh
./build/resiris-tests             # 156 Catch2 cases
./tools/differential_test.sh      # 29 programs, compared with PyResy
python3 tools/audit_sweep.py      # inline programs extracted from the PyResy test suite
```

## Documentation

Language syntax, types, modules and hardware notes are in `txt/info/`; design notes are in `txt/ideas/`.
