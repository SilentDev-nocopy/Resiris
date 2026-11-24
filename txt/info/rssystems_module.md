# RSSystem module

```resiris
<include> RSSystem
```

`RSSystem` reports runtime facts and ESP32 hardware information. All functions take no arguments; the module exports no user constants and creates no `ModuleObject`s.

> [!IMPORTANT]
> The CLI is a desktop program. Its hardware-only functions raise a `ModuleError` instead of returning fabricated values. The portable functions work here: the CLI installs a program clock when it starts a run.

## 1. Functions available on every target

| Function | Result | Notes |
| --- | --- | --- |
| `RSSystem.cpu_cores()` | `int` | The host's reported hardware concurrency; errors if it cannot report a non-zero count. |
| `RSSystem.cpu_load()` | `float` | Percent of wall-clock time the CLI spent executing, `0..100`. |
| `RSSystem.uptime()` | `float` | Seconds since the current program run started, not since boot. |
| `RSSystem.modules()` | `string` | Comma-separated names of all modules registered in this build, including modules the script never includes. |
| `RSSystem.report()` | `string` | Formatted summary. On a PC it omits the hardware rows. |

`cpu_load()`, `uptime()`, and `report()` require the host to have installed a program clock. The Resiris CLI installs one; an application that embeds the library without one gets `RSSystem.<function>: no host installed a program clock, so it has no answer`.

## 2. ESP32-only functions

These read the device's own ESP32-WROOM-32 hardware. On the CLI they throw `RSSystem.<function>: reads the ESP32's own hardware, so it has no answer on a PC`.

| Function | Result |
| --- | --- |
| `RSSystem.chip_model()` | `string`, chip model name |
| `RSSystem.chip_revision()` | `int`, silicon revision |
| `RSSystem.cpu_speed()` | `int`, CPU frequency in MHz |
| `RSSystem.memory_total()` | `int`, total heap bytes |
| `RSSystem.memory_free()` | `int`, currently free heap bytes |
| `RSSystem.memory_used()` | `int`, total heap minus free heap |
| `RSSystem.memory_min_free()` | `int`, lowest free heap observed since boot |
| `RSSystem.psram_size()` | `int`, PSRAM bytes; 0 if there is no PSRAM |
| `RSSystem.flash_total()` | `int`, physical flash capacity in bytes |
| `RSSystem.flash_used()` | `int`, size of the application partition in bytes |

`flash_used()` reports the application partition's reserved capacity; it is not a count of bytes currently occupied by firmware or filesystem data.

## 3. Metadata

The registry answers `RSSystem[NAME]`, `RSSystem[FUNCTIONS]`, and `RSSystem[VARIABLES]`. Every other constant is unknown. The module obtains the registered module list through `Module::on_registered()`, so `modules()` includes `RSSystem` itself.

## 4. Example

```resiris
<include> RSSystem

print_cmd(str(RSSystem.cpu_cores()))
print_cmd(str(RSSystem.uptime()))
print_cmd(RSSystem.modules())
print_cmd(RSSystem.report())
```

`tests/test_rssystem.cpp` verifies the portable facts, the PC hardware errors, the report format, and the missing-clock behavior. The firmware-side implementation with real hardware reads lives in the Galena project.
