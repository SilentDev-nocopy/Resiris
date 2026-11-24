# Native module system

## 1. Modules

A Resiris **module** is a native C++ component that can be used from a Resiris program. It provides functions, constants, or objects.

Current modules:

- `RSMath`
- `RSBase`
- `RSConvert`
- `RSSignal`
- `RSNetwork`

The modules are **compiled into the application**. Resiris does not load module files dynamically from a `Modules` folder.

> [!NOTE]
> The CLI is a desktop program. In this build `RSSignal` is a stateful simulation; it does not access physical ESP32 pins or an ADC. See [rssignal_module.md](rssignal_module.md) for its simulated behavior and [esp32_gpio_table.md](esp32_gpio_table.md) for hardware planning information. `RSNetwork`'s Wi-Fi functions throw a `ModuleError` on the CLI; its HTML server works on `127.0.0.1`. See [rsnetwork_module.md](rsnetwork_module.md).

## 2. Including a module

`<include>` includes a module that is registered in the C++ module registry. A module must be included **before** its functions or constants are used. The include statement does not have to be on the first line of the program.

```resiris
<include> RSMath
<include> RSBase
<include> RSMath, RSBase
```

| Situation | Error |
| --- | --- |
| Missing module | `{ModuleName} not found! Error code:"MissingModule"` |
| Including the same module more than once | `{ModuleName} is already included. Error code:"SameModuleMultiCall"` |

## 3. Using module functions and constants

| Item | Syntax | Example |
| --- | --- | --- |
| Module functions | Module name and a dot | `RSMath.sqrt(25)` |
| Module constants | Square brackets | `RSMath[PI]`, `RSMath[E]` |

| Situation | Error |
| --- | --- |
| Unknown function | `{ModuleName}.{function}: unknown module function` |
| Unknown constant | `{ModuleName}.{constant}: unknown module constant` |

## 4. ModuleObject

A module can return an object that is stored in a `ModuleObject` variant:

```resiris
v timer ModuleObject = RSBase.await()
```

The methods of a `ModuleObject` are called with dot syntax:

```resiris
timer.set_timer(1.0)
timer.process()
```

The interpreter sends the method call to the module that owns the object.

## 5. C++ module interface

A C++ module implements the `Module` interface from `include/resiris/module.hpp`. The interface contains six pure virtual methods:

- `module_name()`
- `function_names()`
- `variables()`
- `call_function(...)`
- `call_object_method(...)`
- `get_constant(...)`

Object method names must also appear in `function_names()`. The CLI constructs the modules explicitly in `src/cli.cpp`.

The module provides its exported constants through `variables()` and `get_constant()`. The type declared in `variables()` is checked against the returned value.

The module registry also provides these metadata values:

| Metadata | Meaning |
| --- | --- |
| `NAME` | The module name. |
| `FUNCTIONS` | A comma-separated list of function names. |
| `VARIABLES` | A comma-separated list of `name:type` entries. |

These three values are provided by `ModuleRegistry` and do not need to be implemented by `get_constant()`.

## 6. Function arguments and return values

| Direction | Supported types |
| --- | --- |
| Function arguments | `bool`, `int`, `float`, `string` |
| Function return values | `bool`, `int`, `float`, `string`, `ModuleObject` |

Other argument types are rejected with `UnknownModuleArgument`. Other return types are rejected as well.

A module function must check its own argument count and any function-specific argument types. For an invalid function signature it throws `ModuleSignatureError`.

The registry only permits `bool`, `int`, `float`, and `string` arguments. It permits `bool`, `int`, `float`, `string`, and `ModuleObject` results. Constants must match the type declared by `variables()`.

## 7. Adding a new C++ module

1. Implement the `Module` interface.
2. Add the source file to `CMakeLists.txt`.
3. Include its header in `src/cli.cpp`.
4. Register an instance in the vector passed to `ModuleRegistry`.
5. Add tests and a module reference under `txt/info/`.
6. Add the module and its reference to the current-module list in [language_reference.md](language_reference.md), this document, and `README.md`.

## 8. Current limitations

- Modules are registered by the C++ application.
- Loading modules from a folder at runtime is not implemented.
- Module dependencies and version handling are not implemented.
- There is no external C ABI or dynamic plugin file format.
- Only modules registered by the host application can be used.
