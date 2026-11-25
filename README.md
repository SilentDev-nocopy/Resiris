# Resiris

<img src="assets/resiris-file-icon.png" alt="Resiris file icon" width="96" align="right">

Resiris is a **statically typed, high-level scripting language for embedded
devices**, aimed primarily at the ESP32. It also runs on a desktop through the
`resiris` CLI, so the same `.resy` file can be tested on a PC and then flashed
unchanged.

Its surface is deliberately low-level: no loops, no arrays, no pointers and no
garbage collector. There is no `main`; `START()` runs once and `PROCESS(FPS)`
runs every frame. Hardware and protocol work lives in a small set of modules.

Current version: `1.10-cpp`, defined in
[`include/resiris/version.hpp`](include/resiris/version.hpp).

## Build

Requires a C++17 compiler and CMake 3.16 or newer.

```sh
cmake -S . -B build
cmake --build build -j
```

## Run

```sh
./build/resiris examples/01_basics.resy
```

The timer demos in `examples/` run until Ctrl+C. `resiris --help` lists the
options, `resiris --version` prints the version, and running without a file
shows usage.

## Language basics

| | |
|---|---|
| Types | `int`, `float`, `string`, `bool`, `ModuleObject`, `FunctionalObject`, `UnknownObject` |
| Declaration | `v` variable, `c` constant, `fn` function |
| Control flow | `if` / `elif` / `else`, `mat`, `return` |
| Lifecycle | `START()`, `PROCESS(FPS)` |
| Modules | six built-ins, listed below |

```resiris
<include> RSBase

c FPS float = 4.0
v ticks int = 0

START():
	led.set_timer(0.5)
	led.on_timeout("blink")

PROCESS(FPS):
	ticks += 1
	print_cmd(ticks)
	print_cmd(led.process())
```

Rules worth knowing:

- Indentation uses tabs; spaces are rejected.
- `7 / 2` is an error. Use a float operand for division.
- Errors report `line N, column M`.
- On the ESP32, build long strings with `+=`. Each `+` recurses in the
  interpreter and a long chain can overflow the `loopTask` stack.

## Modules

| Module | What it provides |
|---|---|
| `RSBase` | timers and `await` |
| `RSConvert` | text, number and binary conversions |
| `RSMath` | numeric functions |
| `RSNetwork` | ESP32 Wi-Fi and the portable HTML server |
| `RSSignal` | GPIO input and output |
| `RSSystem` | device facts and hardware reports |

Wi-Fi and the `RSSystem` hardware functions are ESP32 only; the CLI raises a
`ModuleError` for them. The HTML server works on a PC at `127.0.0.1`. See
[`txt/info/`](txt/info/) for the full references.

## Install / uninstall

```sh
cmake --install build --prefix ~/.local   # ~/.local/bin/resiris
tools/uninstall.sh                        # --purge also removes build/ and local settings
```

## Tests

```sh
./build/resiris-tests             # 156 Catch2 cases
./tools/differential_test.sh      # 29 programs compared with PyResy
python3 tools/audit_sweep.py      # needs a PyResy checkout
```

If PyResy is not next to this checkout, set `RESIRIS_PROTOTYPE_DIR`.

## Documentation

- [`txt/info/`](txt/info/) — syntax, types, modules, hardware notes
- [`txt/ideas/`](txt/ideas/) — design notes
- [Galena](https://github.com/SilentDev-nocopy/Galena.git) — the device this
  language was written for
