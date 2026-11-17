#include "resiris/value.hpp"

#include <charconv>
#include <cstring>

namespace resiris {

std::string float_to_string(double value) {
    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc()) {
        return "nan";
    }
    std::string out(buffer, result.ptr);

    // Python's repr appends ".0" to integral floats that printed in decimal
    // form, e.g. repr(2.0) == "2.0". Values printed in scientific notation
    // keep their exponent.
    if (out.find('.') == std::string::npos &&
        out.find('e') == std::string::npos &&
        out.find('E') == std::string::npos &&
        out != "inf" && out != "-inf" && out != "nan") {
        out += ".0";
    }
    return out;
}

std::string value_to_string(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: return value.string_value;
        case Value::Type::ModuleObject:
            return value.module_value ? value.module_value->to_string() : "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "None";
    }
}

std::string value_repr(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: {
            std::string out = "'";
            for (char ch : value.string_value) {
                switch (ch) {
                    case '\\': out += "\\\\"; break;
                    case '\'': out += "\\'"; break;
                    case '\n': out += "\\n"; break;
                    case '\t': out += "\\t"; break;
                    default: out.push_back(ch); break;
                }
            }
            out += "'";
            return out;
        }
        case Value::Type::ModuleObject:
            return value.module_value ? value.module_value->to_string() : "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "None";
    }
}

std::string ModuleObject::to_string() const {
    return "ModuleObject(" + module_name + ")";
}

bool operator==(const Value& a, const Value& b) {
    if (a.type == Value::Type::Bool && b.type == Value::Type::Bool) {
        return a.bool_value == b.bool_value;
    }
    if (a.type == Value::Type::Bool || b.type == Value::Type::Bool) {
        return false;
    }
    if (a.type == Value::Type::Int && b.type == Value::Type::Int) {
        return a.int_value == b.int_value;
    }
    if (a.type == Value::Type::Int && b.type == Value::Type::Float) {
        return static_cast<double>(a.int_value) == b.float_value;
    }
    if (a.type == Value::Type::Float && b.type == Value::Type::Int) {
        return a.float_value == static_cast<double>(b.int_value);
    }
    if (a.type == Value::Type::Float && b.type == Value::Type::Float) {
        return a.float_value == b.float_value;
    }
    if (a.type == Value::Type::String && b.type == Value::Type::String) {
        return a.string_value == b.string_value;
    }
    if (a.type == Value::Type::ModuleObject && b.type == Value::Type::ModuleObject) {
        return a.module_value == b.module_value;
    }
    if (a.type == Value::Type::FunctionalObject && b.type == Value::Type::FunctionalObject) {
        return a.function_value == b.function_value;
    }
    return false;
}

} // namespace resiris