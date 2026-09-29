#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "resiris/errors.hpp"

namespace resiris {

class Statement;

// Base for module-owned state objects that take part in the frame clock /
// callback protocol (the RSBase timer handle). The interpreter injects `now`
// before every object method call and fires `fire_callback` once afterwards.
struct FrameAwareState {
    double now = 0.0;
    std::optional<std::string> fire_callback;
    virtual ~FrameAwareState() = default;
};

/*
 * A module-owned object returned by a module function. `handle` points at the
 * runtime state the module manages for this object.
 */
class ModuleObject {
public:
    ModuleObject() = default;
    ModuleObject(std::string name, std::shared_ptr<FrameAwareState> h)
        : module_name(std::move(name)), handle(std::move(h)) {}

    std::string module_name;
    std::shared_ptr<FrameAwareState> handle;

    std::string to_string() const;
};

/*
 * Runtime representation of a FunctionalObject value.
 */
class FunctionalObject {
public:
    std::vector<std::string> parameters;
    std::vector<std::shared_ptr<Statement>> body;
};

// Python int stays exact; arbitrary precision is not required by the prototype.
using Int = std::int64_t;

/*
 * Dynamic Resiris value.
 */
class Value {
public:
    enum class Type : std::uint8_t {
        Undefined,
        Int,
        Float,
        String,
        Bool,
        ModuleObject,
        FunctionalObject,
    };

    Type type = Type::Undefined;
    Int int_value = 0;
    double float_value = 0.0;
    bool bool_value = false;
    std::string string_value;
    std::shared_ptr<ModuleObject> module_value;
    std::shared_ptr<FunctionalObject> function_value;

    static Value undefined() { return Value(); }

    static Value make_int(Int value) {
        Value v;
        v.type = Type::Int;
        v.int_value = value;
        return v;
    }

    static Value make_float(double value) {
        Value v;
        v.type = Type::Float;
        v.float_value = value;
        return v;
    }

    static Value make_string(std::string value) {
        Value v;
        v.type = Type::String;
        v.string_value = std::move(value);
        return v;
    }

    static Value make_bool(bool value) {
        Value v;
        v.type = Type::Bool;
        v.bool_value = value;
        return v;
    }

    static Value make_module(std::shared_ptr<ModuleObject> value) {
        Value v;
        v.type = Type::ModuleObject;
        v.module_value = std::move(value);
        return v;
    }

    static Value make_function(std::shared_ptr<FunctionalObject> value) {
        Value v;
        v.type = Type::FunctionalObject;
        v.function_value = std::move(value);
        return v;
    }

    bool is_number() const {
        return (type == Type::Int || type == Type::Float);
    }

    std::string type_name() const {
        switch (type) {
            case Type::Bool: return "bool";
            case Type::Int: return "int";
            case Type::Float: return "float";
            case Type::String: return "string";
            case Type::ModuleObject: return "ModuleObject";
            case Type::FunctionalObject: return "FunctionalObject";
            default: return "Undefined";
        }
    }
};

// Internal signal used to execute return.
class ReturnSignal {
public:
    explicit ReturnSignal(std::optional<Value> value) : value(std::move(value)) {}

    std::optional<Value> value;
};

/*
 * Convert a runtime value to the Resiris string representation.
 * Mirrors Python's str() used by print_cmd and `string` conversion.
 */
std::string value_to_string(const Value& value);
std::string float_to_string(double value);
// Python repr()-style rendering of a runtime value.
std::string value_repr(const Value& value);

bool operator==(const Value& a, const Value& b);
inline bool operator!=(const Value& a, const Value& b) { return !(a == b); }

} // namespace resiris