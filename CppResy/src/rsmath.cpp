#include "resiris/rsmath.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace resiris {

namespace {

// Python math functions accept bool as a number (True == 1).
bool is_number(const Value& v) {
    return v.type == Value::Type::Int || v.type == Value::Type::Float ||
           v.type == Value::Type::Bool;
}

double as_number(const Value& v) {
    switch (v.type) {
        case Value::Type::Int: return static_cast<double>(v.int_value);
        case Value::Type::Bool: return v.bool_value ? 1.0 : 0.0;
        default: return v.float_value;
    }
}

Int as_int(const Value& v) {
    switch (v.type) {
        case Value::Type::Int: return v.int_value;
        case Value::Type::Bool: return v.bool_value ? 1 : 0;
        default: return static_cast<Int>(v.float_value);
    }
}

const Value& require(const std::vector<Value>& args, std::size_t index, int kind) {
    if (index >= args.size()) {
        throw ModuleSignatureError();
    }
    const Value& v = args[index];
    bool ok = false;
    if (kind == 0) {        // any number
        ok = is_number(v);
    } else if (kind == 1) {  // exactly int-valued (int or bool)
        ok = v.type == Value::Type::Int || v.type == Value::Type::Bool;
    } else {                 // string
        ok = v.type == Value::Type::String;
    }
    if (!ok) {
        throw ModuleSignatureError();
    }
    return v;
}

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

// Python truthiness used by `mod` style operators is unnecessary here; this is
// the Python `%` (floor modulo) preserved for negative operands.
double python_modulo(double left, double right) {
    return left - std::floor(left / right) * right;
}

bool numeric_lt(const Value& a, const Value& b) {
    return as_number(a) < as_number(b);
}

} // namespace

Value RsMathModule::call_function(const std::string& fn, const std::vector<Value>& args) {
    if (fn == "abs") {
        require_count(args, 1);
        const Value& v = require(args, 0, 0);
        if (v.type == Value::Type::Float) {
            return Value::make_float(std::fabs(v.float_value));
        }
        return Value::make_int(std::abs(as_int(v)));
    }

    if (fn == "sqrt") {
        require_count(args, 1);
        double v = as_number(require(args, 0, 0));
        if (v < 0) {
            throw ResirisError("math domain error");
        }
        return Value::make_float(std::sqrt(v));
    }

    if (fn == "cbrt") {
        require_count(args, 1);
        return Value::make_float(std::cbrt(as_number(require(args, 0, 0))));
    }

    if (fn == "pow") {
        require_count(args, 2);
        return Value::make_float(
            std::pow(as_number(require(args, 0, 0)), as_number(require(args, 1, 0))));
    }

    if (fn == "floor") {
        require_count(args, 1);
        return Value::make_int(static_cast<Int>(std::floor(as_number(require(args, 0, 0)))));
    }

    if (fn == "ceil") {
        require_count(args, 1);
        return Value::make_int(static_cast<Int>(std::ceil(as_number(require(args, 0, 0)))));
    }

    if (fn == "round") {
        // Python builtins.round: round-half-even, returns Int.
        require_count(args, 1);
        double v = as_number(require(args, 0, 0));
        double rounded = std::round(v);
        // Break ties towards even.
        if (std::fabs(v - rounded) == 0.5) {
            rounded = 2.0 * std::floor((v + 0.5) / 2.0);
        }
        return Value::make_int(static_cast<Int>(rounded));
    }

    if (fn == "ln") {
        require_count(args, 1);
        double v = as_number(require(args, 0, 0));
        if (v <= 0.0) {
            throw ResirisError("math domain error");
        }
        return Value::make_float(std::log(v));
    }

    if (fn == "log10") {
        require_count(args, 1);
        return Value::make_float(std::log10(as_number(require(args, 0, 0))));
    }

    if (fn == "sin") {
        require_count(args, 1);
        return Value::make_float(std::sin(as_number(require(args, 0, 0))));
    }

    if (fn == "cos") {
        require_count(args, 1);
        return Value::make_float(std::cos(as_number(require(args, 0, 0))));
    }

    if (fn == "tan") {
        require_count(args, 1);
        return Value::make_float(std::tan(as_number(require(args, 0, 0))));
    }

    if (fn == "asin") {
        require_count(args, 1);
        return Value::make_float(std::asin(as_number(require(args, 0, 0))));
    }

    if (fn == "acos") {
        require_count(args, 1);
        return Value::make_float(std::acos(as_number(require(args, 0, 0))));
    }

    if (fn == "atan") {
        require_count(args, 1);
        return Value::make_float(std::atan(as_number(require(args, 0, 0))));
    }

    if (fn == "hypot" || fn == "pythagoras") {
        require_count(args, 2);
        return Value::make_float(
            std::hypot(as_number(require(args, 0, 0)), as_number(require(args, 1, 0))));
    }

    if (fn == "factorial") {
        require_count(args, 1);
        Int v = as_int(require(args, 0, 1));
        if (v < 0) {
            throw ResirisError("math domain error");
        }
        Int result = 1;
        for (Int i = 2; i <= v; ++i) {
            result *= i;
        }
        return Value::make_int(result);
    }

    if (fn == "ncr") {
        require_count(args, 2);
        Int n = as_int(require(args, 0, 1));
        Int r = as_int(require(args, 1, 1));
        if (n < 0 || r < 0) {
            throw ResirisError("math domain error");
        }
        if (r > n) {
            return Value::make_int(0);
        }
        Int result = 1;
        r = std::min(r, n - r);
        for (Int i = 0; i < r; ++i) {
            result = result * (n - i) / (i + 1);
        }
        return Value::make_int(result);
    }

    if (fn == "npr") {
        require_count(args, 2);
        Int n = as_int(require(args, 0, 1));
        Int r = as_int(require(args, 1, 1));
        if (n < 0 || r < 0) {
            throw ResirisError("math domain error");
        }
        if (r > n) {
            return Value::make_int(0);
        }
        Int result = 1;
        for (Int i = 0; i < r; ++i) {
            result *= (n - i);
        }
        return Value::make_int(result);
    }

    if (fn == "gcd") {
        require_count(args, 2);
        Int a = as_int(require(args, 0, 1));
        Int b = as_int(require(args, 1, 1));
        return Value::make_int(std::gcd(a, b));
    }

    if (fn == "lcm") {
        require_count(args, 2);
        Int a = as_int(require(args, 0, 1));
        Int b = as_int(require(args, 1, 1));
        if (a == 0 || b == 0) {
            return Value::make_int(0);
        }
        Int g = std::gcd(a, b);
        return Value::make_int((a / g) * b);
    }

    if (fn == "mod") {
        require_count(args, 2);
        const Value& left = require(args, 0, 0);
        const Value& right = require(args, 1, 0);
        double divisor = as_number(right);
        if (divisor == 0.0) {
            throw ResirisError("modulo by zero");
        }
        if (left.type == Value::Type::Int && right.type == Value::Type::Int) {
            return Value::make_int(static_cast<Int>(python_modulo(
                static_cast<double>(left.int_value), static_cast<double>(right.int_value))));
        }
        return Value::make_float(python_modulo(as_number(left), divisor));
    }

    if (fn == "min") {
        require_count(args, 2);
        const Value& a = require(args, 0, 0);
        const Value& b = require(args, 1, 0);
        if (a.type == Value::Type::String && b.type == Value::Type::String) {
            return a.string_value <= b.string_value ? a : b;
        }
        if (is_number(a) && is_number(b)) {
            return numeric_lt(a, b) ? a : b;
        }
        throw ModuleSignatureError();
    }

    if (fn == "max") {
        require_count(args, 2);
        const Value& a = require(args, 0, 0);
        const Value& b = require(args, 1, 0);
        if (a.type == Value::Type::String && b.type == Value::Type::String) {
            return a.string_value >= b.string_value ? a : b;
        }
        if (is_number(a) && is_number(b)) {
            return numeric_lt(a, b) ? b : a;
        }
        throw ModuleSignatureError();
    }

    if (fn == "clamp") {
        require_count(args, 3);
        const Value& value = require(args, 0, 0);
        const Value& minimum = require(args, 1, 0);
        const Value& maximum = require(args, 2, 0);
        if (!(is_number(value) && is_number(minimum) && is_number(maximum))) {
            throw ModuleSignatureError();
        }
        Value intermediate = numeric_lt(value, minimum) ? minimum : value;
        return numeric_lt(maximum, intermediate) ? maximum : intermediate;
    }

    if (fn == "is_nan") {
        require_count(args, 1);
        return Value::make_bool(std::isnan(as_number(require(args, 0, 0))));
    }

    if (fn == "is_inf") {
        require_count(args, 1);
        return Value::make_bool(std::isinf(as_number(require(args, 0, 0))));
    }

    if (fn == "is_finite") {
        require_count(args, 1);
        return Value::make_bool(std::isfinite(as_number(require(args, 0, 0))));
    }

    if (fn == "pythagoras") {
        require_count(args, 2);
        return Value::make_float(
            std::hypot(as_number(require(args, 0, 0)), as_number(require(args, 1, 0))));
    }

    throw ModuleError("RSMath." + fn + ": unknown module function");
}

Value RsMathModule::get_constant(const std::string& name) const {
    if (name == "PI") {
        return Value::make_float(3.141592653589793);
    }
    if (name == "E") {
        return Value::make_float(2.718281828459045);
    }
    throw ModuleError("RSMath." + name + ": unknown module constant");
}

} // namespace resiris