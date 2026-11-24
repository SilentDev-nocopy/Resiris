# The `mat` statement

`mat` is a built-in statement that compares a value against multiple possible cases. It is **not** a function or a module.

## Basic syntax

```resiris
mat expression:
    1:
        print_cmd("one")
    2:
        print_cmd("two")
    else:
        print_cmd("other")
```

The expression after `mat` must **not** be a literal by itself. A variable, conversion, supported call, or other expression may be used if the resulting value has a determinable runtime type.

## What can be checked?

**Can the value be a normal variable without using `.type()`?**

Yes. It can check values with a determinable runtime type, including `int`, `float`, `string`, `bool`, and `ModuleObject`.

**Can it check a value by its type using `.type()`?**

Yes:

```resiris
mat variant_name.type():
    int:
        print_cmd("Is int")
    float:
        print_cmd("Is float")
```

**Which values can be checked?**

`int`, `float`, `string`, `bool`, and `ModuleObject` values can be checked. `FunctionalObject` cannot be checked because the interpreter cannot infer its type. An `UnknownObject` containing a scalar or `ModuleObject` can be checked because its stored runtime value has a concrete type.

**Can it check an `UnknownObject` value?**

Yes, if it contains a value with a determinable runtime type. `UnknownObject` is a declaration type; it is not the runtime type of the stored value.

**Can it check a literal directly?**

No. The value after `mat` must be a variable or a supported expression.

**Can it check the result of `.type()`?**

Yes.

**Can it check the result of `.string()`?**

Yes.

**Can it check the result of another function?**

Yes, if the expression is accepted by the expression grammar and produces a value whose type the interpreter can determine.

**Is the value evaluated once or multiple times?**

It is evaluated **once**.

**What happens if the value does not exist?**

The interpreter reports an error because the value cannot be found.

## Case values

**What can be used as a case value?**

Cases use fixed values. Supported case values are `int`, `float`, `string`, and `bool`.

**Can a case contain an expression?**

No.

**Can a case use a variable or constant instead of a fixed value?**

No. A case must contain a concrete value.

**Can the same case value appear more than once?**

No. A duplicate case gives an error: `SameCaseMultiCall`.

**Can different types be used as cases in the same `mat`?**

No. All cases in one `mat` must use the same type.

**What happens if a case has a different type from the checked value?**

It does not match. If no case matches, the `else` branch runs if one was written; otherwise execution continues after the `mat`.

**Is type matching based on exact type equality?**

Yes. An `int` case is not the same as a `float` case.

**Does the order of cases matter?**

No. The cases are compared by their values.

## Structure and control flow

**Can `mat` contain multiple cases?**

Yes.

**Can it contain an `else` branch?**

Yes. The `else` branch must be the **last** branch.

**Can there be more than one `else` branch?**

No.

**Can it have no cases and only an `else` branch?**

No. At least one case is required.

**What happens when a case matches?**

The matching branch runs. After that, the `mat` ends and execution continues with the next line after the whole `mat`. There is **no fall-through**.

**What happens when no case matches and there is no `else`?**

Execution continues after the `mat`.

**Can cases contain multiple lines of code?**

Yes. Every statement in the matching branch runs, unless a `return` or an error stops it.

**Can `return` be used inside a case?**

Yes. It returns from the surrounding function.

**Can `pass` be used inside a case?**

Yes. It does nothing.

## Nesting and placement

**Can a case contain another `mat`?**

No. A `mat` cannot be nested inside another `mat` branch; it gives `NestedMatchError`.

**Where can `mat` be used?**

It can be used inside functions, inside `if`/`elif`/`else` blocks, and inside `START()` or `PROCESS(FPS)`.

## Errors

| Situation | Result |
| --- | --- |
| Invalid case value | The parser reports `InvalidCaseValue`. |
| Case has no body | The parser reports `MissingCaseBody`. |
| Empty `mat` body | The parser reports `EmptyMatchBody`. |
| Invalid `mat` syntax | The parser reports a syntax error. |
| Invalid indentation in a case | The parser reports an indentation error. |
| Error while the matching case is running | The case stops and the error is passed back to the surrounding execution with `MatchCaseExecutionError` appended. |

## Example

```resiris
fn describe(value):
    mat value:
        1:
            return "one"
        2:
            return "two"
        else:
            return "other"

print_cmd(describe(2))
```

## Implementation notes

This description follows the current Resiris parser/interpreter.

- The checked expression is evaluated once.
- Case values must be fixed literals.
- All cases in one `mat` must use the same type.
- Duplicate cases are rejected.
- Nested `mat` statements are not supported.
- There is no fall-through between cases.

> [!NOTE]
> Do not treat older ideas or features that are not implemented in Resiris as part of the current `mat` behaviour.
