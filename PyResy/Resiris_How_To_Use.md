# Resiris — How To Use

Resiris is the programming language used by EspCalc.

This document explains the currently defined Resiris language features and their intended usage.

---

## 1. Comments

Resiris uses `##` for comments.

```resy
## This is a comment
```

---

## 2. Variants

A Resiris variable is called a **variant** and is declared with `v`.

```resy
v number int = 10
```

The type of the variant is part of its declaration.

---

## 3. Constants

Constants are declared with `c`.

```resy
c number int = 10
```

A constant cannot be modified after its value has been assigned.

---

## 4. Types

The currently defined Resiris types are:

- `UnknownObject`
- `int`
- `float`
- `string`
- `bool`
- `ModuleObject`
- `FunctionalObject`

An `UnknownObject` receives its type when its first value is assigned.

---

## 5. Assignment

Resiris supports normal assignment and the following compound assignments:

```resy
=
+=
-=
*=
/=
```

Example:

```resy
v number int = 10

number += 5
number -= 2
number *= 2
number /= 2
```

Constants cannot be reassigned.

---

## 6. Functions

Functions are defined with `fn`.

```resy
fn calculate():
	## function body
```

Function parameters and local variables belong to the function's local scope.

Global variants can be read from a function.

---

## 7. `START()`

`START()` is the program entry point and runs once when the program starts.
It is a lifecycle declaration, not a normal `fn`.

```resy
START():
	print_cmd("Program started")
```

---

## 8. `PROCESS(FPS)`

`PROCESS(FPS)` is the repeating lifecycle declaration. The parameter is always named `FPS` and has type `float`.
The global program constant `c FPS float` determines how often the lifecycle runs.

```resy
c FPS float = 30.0

PROCESS(FPS):
	print_cmd(FPS)
```

The timing is handled by the runtime; the Resiris program does not calculate a `delta` value.

---

## 9. `return`

`return` is used inside a function.

It immediately exits the current function.

Example:

```resy
fn calculate():
	return 10
```

A `return` can also be used inside a `mat` case.

---

## 10. `pass`

`pass` is a no-op statement.

It does not stop execution and does not skip the rest of the block.

```resy
if condition:
	pass
	print_cmd("This still executes")
```

`pass` can also be used inside a `mat` case.

---

## 11. `if`, `elif`, `else`

Resiris supports conditional control flow with:

```resy
if
elif
else
```

Example:

```resy
if value:
	print_cmd("condition is true")
elif other_value:
	print_cmd("other condition is true")
else:
	print_cmd("no condition matched")
```

Blocks are defined by indentation.

---

## 12. `mat`

`mat` is a **control-flow statement**.

It compares one existing value against multiple concrete case values and executes the matching branch.

`mat` is not a function or module.

### Basic syntax

```resy
mat value:
	1:
		print_cmd("one")
	2:
		print_cmd("two")
	else:
		print_cmd("other")
```

The value checked by `mat` must already exist. A literal cannot be used directly as the value being checked.

### Matching

The value checked by `mat` is evaluated once.

The case values are concrete values.

When a case matches:

1. The matching branch executes.
2. Every statement in that branch executes.
3. The `mat` ends.
4. Execution continues after the `mat`.

There is no fall-through.

If no case matches and an `else` exists, the `else` branch executes.

If no case matches and there is no `else`, execution continues after the `mat`.

### Types

`mat` can check values of the defined Resiris types, except `UnknownObject`.

Case types must match the checked value exactly.

Different case types cannot be mixed in the same `mat`.

A case whose type does not match the checked value is skipped.

### Type matching

`.type()` can be used when the type itself is what should be matched.

```resy
mat value.type():
	int:
		print_cmd("Is int")
	float:
		print_cmd("Is float")
```

### String matching

`.string()` can also produce the value checked by `mat`.

### Case bodies

A case can contain multiple statements.

`return` and `pass` can be used inside a case.

### Nesting

`mat` can be used inside functions and conditional blocks.

A `mat` cannot be nested inside another `mat` case.

---

## 13. Built-in `type()`

`type()` is a built-in function used for type conversion and type querying.

It can be called on a variant or constant.

### Type querying

With no argument, `.type()` returns the type of the value.

```resy
v number int = 10

print_cmd(number.type())
```

### Type conversion

A value can be converted with:

```resy
value.type(type)
```

The supported conversion targets are:

```text
int
float
string
bool
```

The original value is not modified. The converted value is returned.

Examples:

```resy
v number int = 10

v decimal float = number.type(float)
```

```resy
v number int = 10

v text string = number.type(string)
```

Boolean conversion rules:

```text
true  -> 1
false -> 0
```

for `int`, and the corresponding floating-point values for `float`.

Boolean-to-string conversion produces:

```text
true
false
```

Integer and float values can be converted to `string`.

String values cannot be converted to `int`, `float`, or `bool`.

---

## 14. Built-in `string()`

`.string()` is a built-in conversion function.

It produces the string representation of the value.

It can be used on values and constants.

Example:

```resy
v number int = 42

print_cmd(number.string())
```

The result can also be used by `mat`:

```resy
v number int = 42

mat number.string():
	"42":
		print_cmd("matched")
```

---

## 15. `print_cmd()`

`print_cmd()` is a Resiris built-in/module function used to output a value.

Example:

```resy
print_cmd("Hello")
```

It is a function, not a Resiris keyword.

---

## 16. Includes and Modules

Modules are included with `include`.

```resy
include ModuleName
```

Multiple modules can be included on one line using commas:

```resy
include ModuleName, ModuleName2
```

A module is part of the Resiris module system rather than a normal Resiris object.

After inclusion, the module provides its defined functions and values.

Module functions are called using the module name:

```resy
ModuleName.module_function_name(argument)
```

Module constants are accessed with brackets:

```resy
ModuleName[constansname]
```

---

## 17. Module arguments and values

Module function arguments can be:

- `int`
- `float`
- `bool`
- `string`

Module functions can return Resiris values.

Modules can provide:

- functions
- `int`
- `float`
- `string`
- `bool`

Module data does not use `UnknownObject`.

---

## 18. Example Program

The following example combines the currently defined core features:

```resy
## Example Resiris program

v number int = 2
c limit int = 10

fn calculate():
	mat number:
		1:
			print_cmd("one")
		2:
			print_cmd("two")
		else:
			print_cmd("other")

	if number == limit:
		print_cmd("limit reached")
	else:
		print_cmd("limit not reached")

START()
```

---

## 19. Program Structure

A Resiris program is interpreted through the following architecture:

```text
Resiris source
	↓
Tokenizer
	↓
Parser
	↓
AST
	↓
Interpreter
	↓
Runtime
	↓
Modules
```

The language is designed for the EspCalc platform.

The PC interpreter is used as the prototype environment before the final embedded implementation.

---

## 20. Important Syntax Rules

Resiris uses indentation-based blocks.

Blocks use TAB indentation.

`##` starts a comment.

`v` means variant.

`c` means constant.

`fn` defines a function.

`START()` is the program entry point.

`PROCESS(FPS)` is a repeating lifecycle declaration.

`mat` is a control-flow statement.

`print_cmd()` is a function, not a keyword.

---

## 21. Errors

Resiris has defined runtime and language errors for invalid operations.

Examples include:

```text
UnknownVariableError
TypeError
ConstantAssignmentError
MissingValueError
SyntaxError
InvalidInputError
FunctionError
```

Error behavior is part of the language/runtime implementation and is reported when the corresponding invalid operation occurs.
