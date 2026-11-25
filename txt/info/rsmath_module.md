# RSMath module

`RSMath` is compiled into the CLI and works on the desktop without hardware.

```resiris
<include> RSMath
```

It exports the `float` constants `PI` and `E`, has **no object methods**, and currently provides **29 functions**.

## Functions

### Roots and rounding

| Function | Description |
| --- | --- |
| `abs(x)` | Absolute value. |
| `sqrt(x)` | Square root; negative values raise a math-domain error. |
| `cbrt(x)` | Cube root. |
| `pow(x, y)` | `x` raised to `y`. |
| `floor(x)`, `ceil(x)` | Round down/up to an integer. |
| `round(x)` | Nearest integer; halfway values round to even. |

### Logarithms and trigonometry

| Function | Description |
| --- | --- |
| `ln(x)`, `log10(x)` | Natural and base-10 logarithm. `ln` requires `x > 0`. |
| `sin(x)`, `cos(x)`, `tan(x)` | Trigonometric functions; angles are radians. |
| `asin(x)`, `acos(x)`, `atan(x)` | Inverse trigonometric functions; results are radians. |
| `hypot(x, y)` | Hypotenuse of `x` and `y`. |
| `pythagoras(x, y)` | Alias for `hypot(x, y)`. |

### Integer mathematics

| Function | Description |
| --- | --- |
| `factorial(n)` | `n!`; `n` must be a non-negative integer. |
| `ncr(n, r)` | Combinations; inputs must be non-negative integers. |
| `npr(n, r)` | Permutations; inputs must be non-negative integers. |
| `gcd(a, b)` | Greatest common divisor. |
| `lcm(a, b)` | Least common multiple; returns `0` if either input is `0`. |
| `mod(a, b)` | Python-style modulo; `b` must not be `0`. |

### Ranges and predicates

| Function | Description |
| --- | --- |
| `min(a, b)`, `max(a, b)` | Smaller/larger numeric argument. |
| `clamp(value, low, high)` | Constrains `value` to the inclusive bounds. |
| `is_nan(x)`, `is_inf(x)`, `is_finite(x)` | Return `bool` predicates for a numeric value. |

The 29 function names are: `abs`, `sqrt`, `cbrt`, `pow`, `floor`, `ceil`, `round`, `ln`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `hypot`, `factorial`, `ncr`, `npr`, `gcd`, `lcm`, `mod`, `min`, `max`, `clamp`, `is_nan`, `is_inf`, `is_finite`, and `pythagoras`.

## Arguments

- Numeric functions accept `int`, `float`, or `bool` values; `bool` is treated as `1` or `0`.
- Integer functions accept `int` or `bool` values.
- `min` and `max` accept two numbers, not strings.
- A wrong argument count or type raises the standard module signature error.

> [!WARNING]
> Integer results use signed 64-bit `int`. The implementation does **not** validate integer overflow in `factorial`, combinations, permutations, or least common multiple - keep inputs small enough that the result fits. `clamp` expects `low <= high`.

## Example

```resiris
<include> RSMath
print_cmd(RSMath.sqrt(16.0))
print_cmd(RSMath.round(2.5))       ## 2
print_cmd(RSMath.ncr(5, 2))        ## 10
print_cmd(RSMath.mod(-7, 3))       ## 2
print_cmd(RSMath[PI])
```
