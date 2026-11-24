# Resiris language reference

This reference follows the **current** Resiris implementation. A feature that was only discussed or planned is not considered implemented.

## 1. Language syntax

### Comments and indentation

- `##` starts a comment — the tokenizer ignores the whole line.
- Code blocks use **tab-based indentation**. A leading space is not a valid substitute for a tab.
- Text examples in this reference may use visual spaces; source files must use tabs for indentation.
- Statements end at a newline.
- A block starts with a colon (`:`) and its code is indented.

### Declarations and statements

| Syntax | Purpose | Example |
| --- | --- | --- |
| `v` | Defines a variable, also called a *variant*. A type is required. | `v number int = 10`<br>`v speed float = 1.5` |
| `c` | Defines a constant. Its value cannot be changed after definition. | `c FPS float = 10.0` |
| `fn` | Defines a function: a named block of code that can be called later. Parameters do not need a declared type. | See [Functions](#4-functions-and-program-execution) |
| `if` | Checks a condition. Its block runs only when the condition is true. | See [Conditionals](#conditionals) |
| `elif` | Checks another condition when the previous `if`/`elif` condition was false. | See [Conditionals](#conditionals) |
| `else` | Runs when the preceding `if`/`elif` conditions are false. | See [Conditionals](#conditionals) |
| `mat` | Evaluates a non-literal expression once, compares it with fixed cases, and runs the matching branch. See [match_statement.md](match_statement.md). | See [Conditionals](#conditionals) |
| `return` | Stops the current function and optionally returns a value. | `return a + b` |
| `pass` | Performs no operation. It is a placeholder for an empty block; it does not stop the function or ignore code after the block. | `pass` |
| `print_cmd()` | Displays text or a value. | `print_cmd("Hello World")`<br>`print_cmd(test_value)` |
| `<include>` | Includes a module registered in the C++ application. | `<include> RSMath`<br>`<include> RSMath, RSBase` |

```resiris
fn add(a, b):
    return a + b
```

### Conditionals

```resiris
if score >= 10:
    print_cmd("high")
elif score >= 5:
    print_cmd("medium")
else:
    print_cmd("low")

mat value:
    1:
        print_cmd("one")
    2:
        print_cmd("two")
    else:
        print_cmd("other")
```

### Program entry points

| Function | Description |
| --- | --- |
| `START()` | The entry function. It runs once after the top-level code has run. It does not take parameters. |
| `PROCESS(FPS)` | A function that runs on every frame while the program is running. Its parameter must be named `FPS`. A global constant `c FPS float = ...` is required, and its value must be greater than `0.0`. |

## 2. Variables, constants, and value types

### Declaration types

| Type | Description | Example |
| --- | --- | --- |
| `UnknownObject` | A declaration form whose value's type is inferred from its initializer. The value can be a supported scalar or a `ModuleObject`. The inferred type is fixed for later assignments. `FunctionalObject` values must be declared as `FunctionalObject`, not `UnknownObject`. `.type()` works when the stored runtime value has a determinable type; `FunctionalObject` has no supported type conversion. | `v answer UnknownObject = 42` |
| Typed variant | A variant with a declared value type. Every assigned value must use that type. | `v score int = 10` |

### Value types

| Type | Description | Example |
| --- | --- | --- |
| `int` | Stores a whole number as a signed 64-bit integer. | `v lives int = 3` |
| `float` | Stores a number with a fractional part, represented by a C++ `double`. | `v speed float = 1.5` |
| `string` | Stores text. | `v player_name string = "Player"` |
| `bool` | Stores `true` or `false`. | `v is_alive bool = true` |
| `ModuleObject` | Stores an object returned by a native module. | `v timer ModuleObject = RSBase.await()` |
| `FunctionalObject` | Stores a callable value created with `FunctionalObject.new(...)`. | See [FunctionalObject](#functionalobject) |

### Typed assignment

- `int` accepts `int` values.
- `float` accepts `float` and `int` values — an `int` is converted to `float`.
- `string` accepts `string` values only.
- `bool` accepts `bool` values only.
- `ModuleObject` and `FunctionalObject` accept their matching value types.
- `UnknownObject` infers its concrete type at initialization; later assignments must conform to that inferred type.

## 3. Operations and operators

### Arithmetic

| Operator | Description | Example |
| --- | --- | --- |
| `+` | Adds two values. If both values are strings, they are joined together. | `total = number1 + number2` |
| `-` | Subtracts the right value from the left value. | `difference = number1 - number2` |
| `*` | Multiplies two values. | `area = width * height` |
| `/` | Divides the left value by the right value. The left value must be a `float`; an `int` on the left causes `IntDivisionError`. | `v result float = number.type(float) / 2.0` |
| `%` | Returns the remainder after division. | `remainder = number1 % number2` |

### Comparison

| Operator | Description |
| --- | --- |
| `==` | Checks whether two values are equal. The result is `true` or `false`. |
| `!=` | Checks whether two values are different. |
| `>` | Checks whether the left value is greater than the right value. |
| `<` | Checks whether the left value is less than the right value. |
| `>=` | Checks whether the left value is greater than or equal to the right value. |
| `<=` | Checks whether the left value is less than or equal to the right value. |

### Assignment

| Operator | Description | Example |
| --- | --- | --- |
| `=` | Assigns a value to a variant. | `score = 10` |
| `+=` | Adds a value to a variant and stores the result. | `score += 1` |
| `-=` | Subtracts a value from a variant and stores the result. | `score -= 1` |
| `*=` | Multiplies a variant by a value and stores the result. | `score *= 2` |
| `/=` | Divides a variant by a value and stores the result. | `score /= 2` |

### Conversion and inspection

| Operation | Description | Example |
| --- | --- | --- |
| `str()` | Converts a value into text. | `str(test_value)` |
| `.type()` | Gets the type of a value or converts it to another type. See [type_operations.md](type_operations.md) for the supported types and conversion rules. | `number.type(float)` |
| `.string()` | Converts a value into text. | `number.string()` |

## 4. Functions and program execution

### Function

A function is defined with `fn` and can be called by its name.

```resiris
fn greet(name):
    print_cmd("Hello " + name)

greet("Resiris")
```

### Recursion

A function can call itself. Resiris does **not** have traditional `for` or `while` loops.

### FunctionalObject

A callable value can be created with `FunctionalObject.new(...)`:

```resiris
v multiplier FunctionalObject = FunctionalObject.new(value):
    return value * 3

print_cmd(multiplier(4))
```

Its body can access its own parameters and global variables, but it does **not** capture local variables from where it was created.

### Program start

1. Function and lifecycle definitions are registered first.
2. Other top-level statements run in source order.
3. `START()` runs once after the top-level code.
4. The CLI runs `PROCESS(FPS)` repeatedly after `START()`, if it is defined. Its frame clock advances by `1/FPS`; the CLI sleeps between frames to maintain the requested rate, and `Ctrl+C` stops the process loop.

## 5. Modules

| Topic | Description |
| --- | --- |
| Modules | Resiris uses native C++ modules. The CLI currently registers `RSMath`, `RSBase`, `RSConvert`, `RSSignal`, and `RSNetwork`. A program must include a registered module before using it. |
| `<include>` | Includes a registered module. Example: `<include> RSMath, RSBase` |
| Module functions | Called with the module name and a dot. Example: `RSMath.sqrt(25)` |
| Module constants | Accessed with square brackets. Example: `RSMath[PI]` |
| `ModuleObject` | A module can return an object that can be stored in a `ModuleObject` variant. Its methods can then be called with dot syntax. |

See [native_module_system.md](native_module_system.md) for more information.

## 6. Current limitations

- There are no native arrays or collections.
- There are no `for` or `while` loops.
- Continuous execution uses `PROCESS(FPS)`.
- Modules are registered by the C++ application. They are **not** loaded dynamically from a `Modules` folder.
- The current Resiris CLI registers `RSMath`, `RSBase`, `RSConvert`, `RSSignal`, and `RSNetwork`.
- `RSSignal` simulates GPIO state in memory on this desktop build; it does not access physical ESP32 hardware.
- `RSNetwork`'s Wi-Fi functions are ESP32 only and throw a `ModuleError` in the CLI; its HTML server binds to `127.0.0.1`.
- `int` uses a signed 64-bit value.
