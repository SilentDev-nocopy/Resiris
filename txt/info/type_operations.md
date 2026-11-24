# Type operations

## `.type()`

`.type()` is a built-in operation used to **get the type** of a value or **convert** it to another type. It is called on a value:

```resiris
value.type()
value.type(target_type)
```

### How can I get the type of a value?

Call `.type()` without an argument. It returns the type name as a `string`.

```resiris
v number int = 10
print_cmd(number.type())

## Output:
## int
```

### Which types can be checked with `.type()`?

The current Resiris implementation can return:

- `int`
- `float`
- `string`
- `bool`
- `ModuleObject`

An `UnknownObject` declaration stores its inferred runtime value type, so scalar and `ModuleObject` values declared that way can be checked. A `FunctionalObject` **cannot** be checked: the interpreter does not infer a type name for it.

### How can I convert a value to another type?

Give the target type to `.type()`. The original value does **not** change — the operation returns a converted value.

```resiris
v x int = 10
v y float = 2.0

print_cmd(x.type(float))
print_cmd(y.type(bool))
```

### Which target types are supported?

`int`, `float`, `string`, and `bool`.

### Conversion rules

| Conversion | Result |
| --- | --- |
| `int → float` | The integer is converted to a floating-point value: `10 → 10.0`. |
| `float → int` | Resiris converts `(value + 0.5)` to an integer, truncating toward zero as part of the C++ integer conversion. See the examples below. |
| `bool → int` | `true → 1`, `false → 0` |
| `bool → float` | `true → 1.0`, `false → 0.0` |
| `int/float → string` | The numeric value is converted to its text representation. |
| `bool → string` | `true → "true"`, `false → "false"` |
| `int/float → bool` | A value greater than `0` becomes `true`. Zero and negative values become `false`. |
| Same type | The value is returned unchanged. |

`float → int` examples:

```text
 0.4 -> 0
 0.5 -> 1
 1.4 -> 1
 1.5 -> 2
-1.5 -> -1
```

> [!WARNING]
> `float → int` is **not** symmetric rounding for negative values. Out-of-range or non-finite float-to-int conversions are not a supported safe operation.

### Which conversions are not supported?

- `string → int`
- `string → float`
- `string → bool`
- `ModuleObject → int/float/string/bool`
- Other unsupported source types

An unsupported conversion gives `ConversionFail`. In particular, `FunctionalObject` cannot be converted because its source type cannot be inferred by the conversion operation.

### What happens if the target type is invalid?

The interpreter reports `TypeError`.

### Can `.type()` take more than one argument?

No. It accepts **zero** arguments for type checking or **one** argument for conversion.

### Should the target type be written as a string?

No. Write the type name directly:

```resiris
value.type(float)    ## Correct
value.type("float")  ## Incorrect
```

## `.string()`

`.string()` converts the value into a string. It does **not** take arguments and uses the same conversion behaviour as converting to `string`.

```resiris
v number int = 42
print_cmd(number.string())
```

## `str()`

`str(value)` is a built-in function that converts a value into text.

```resiris
print_cmd(str(number))
```

## Important notes

- `.type()` does not modify the original variable.
- Type checking returns a string, not a type object.
- An `UnknownObject` containing a scalar or `ModuleObject` has a determinable runtime type; `FunctionalObject` does not.
- The conversion rules above describe the current Resiris implementation.
