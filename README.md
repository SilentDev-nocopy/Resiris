# Resiris

> Resiris is the programming language designed for [Galena](https://github.com/SilentDev-nocopy/Galena.git), a standalone programmable computing environment.

This repository contains the C++17 tokenizer, parser, AST and interpreter for `.resy` files, along with the `resiris` CLI. The Python prototype (PyResy) is a separate project; it is the reference implementation for the comparison tools below.

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

## Install / uninstall

```sh
cmake --install build --prefix ~/.local   # installs ~/.local/bin/resiris
tools/uninstall.sh                        # --purge also removes build/ and local settings
```

## Test

```sh
./build/resiris-tests             # 140 Catch2 cases
./tools/differential_test.sh      # 29 programs, compared with PyResy
python3 tools/audit_sweep.py      # 589 programs from the Python test suite
```

## Documentation

Language syntax, types, modules and hardware notes are in `txt/info/`; design notes are in `txt/ideas/`.
