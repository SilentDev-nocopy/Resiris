#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "resiris/errors.hpp"
#include "resiris/value.hpp"

namespace resiris {

// Thrown inside a module implementation to signal a Python-TypeError-equivalent
// signature mismatch. The registry converts it into the standard Resiris
// module error message.
class ModuleSignatureError {};

// A native Resiris module (C++ port of the Python prototype modules).
class Module {
public:
    virtual ~Module() = default;

    virtual std::string module_name() const = 0;
    virtual std::vector<std::string> function_names() const = 0;
    virtual std::map<std::string, std::string> variables() const = 0;

    // Plain function call, e.g. RSMath.sqrt(25). `args` are the user
    // arguments. Must throw ModuleSignatureError on signature mismatch.
    virtual Value call_function(const std::string& fn, const std::vector<Value>& args) = 0;

    // Object method call on a ModuleObject value. `handle` is the module-owned
    // object handle; `args` are the user arguments.
    virtual Value call_object_method(const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
                                     const std::vector<Value>& args) = 0;

    // Exported constant access. Throws ModuleError on unknown constant or
    // wrong exported type.
    virtual Value get_constant(const std::string& name) const = 0;

    bool has_function(const std::string& fn) const {
        for (const auto& name : function_names()) {
            if (name == fn) {
                return true;
            }
        }
        return false;
    }
};

// C++ port of the Python-side ModuleLoader. Modules are registered natively
// instead of loaded from Python files.
class ModuleRegistry {
public:
    explicit ModuleRegistry(std::vector<std::shared_ptr<Module>> modules) {
        for (auto& module : modules) {
            available_[module->module_name()] = std::move(module);
        }
    }

    bool is_loaded(const std::string& module_name) const {
        return loaded_.count(module_name) != 0;
    }

    bool has_function(const std::string& module_name, const std::string& fn) const {
        if (loaded_.count(module_name) == 0) {
            return false;
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            return false;
        }
        return found->second->has_function(fn);
    }

    // <include> executes this. Duplicate includes are rejected.
    void load(const std::string& module_name) {
        if (loaded_.count(module_name) != 0) {
            throw ModuleError(
                module_name + " is already included. Error code:\"SameModuleMultiCall\"");
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            throw ModuleError(module_name + " not found! Error code:\"MissingModule\"");
        }
        loaded_.insert(module_name);
    }

    Value call_function(const std::string& module_name, const std::string& fn,
                        const std::vector<Value>& args) {
        Module& module = get(module_name);
        if (!module.has_function(fn)) {
            throw ModuleError(module_name + "." + fn + ": unknown module function");
        }
        validate_arguments(module_name, fn, args);
        Value result;
        try {
            result = module.call_function(fn, args);
        } catch (const ModuleSignatureError&) {
            throw ModuleError(module_name + "." + fn +
                              ": invalid argument count or module function arguments");
        }
        validate_result(module_name, fn, result);
        return wrap_module_result(module_name, result);
    }

    Value call_object_method(const std::string& module_name, const std::shared_ptr<FrameAwareState>& handle,
                             const std::string& method_name, const std::vector<Value>& args) {
        Module& module = get(module_name);
        if (!module.has_function(method_name)) {
            throw ModuleError(module_name + "." + method_name + ": unknown module function");
        }
        validate_arguments(module_name, method_name, args);
        Value result;
        try {
            result = module.call_object_method(method_name, handle, args);
        } catch (const ModuleSignatureError&) {
            throw ModuleError(module_name + "." + method_name +
                              ": invalid argument count or module function arguments");
        }
        validate_result(module_name, method_name, result);
        return wrap_module_result(module_name, result);
    }

    Value get_constant(const std::string& module_name, const std::string& constant_name) {
        Module& module = get(module_name);

        if (constant_name == "NAME") {
            return Value::make_string(module.module_name());
        }
        if (constant_name == "FUNCTIONS") {
            std::string joined;
            bool first = true;
            for (const auto& name : module.function_names()) {
                if (!first) {
                    joined += ", ";
                }
                joined += name;
                first = false;
            }
            return Value::make_string(joined);
        }
        if (constant_name == "VARIABLES") {
            std::string joined;
            bool first = true;
            for (const auto& [name, type] : module.variables()) {
                if (!first) {
                    joined += ", ";
                }
                joined += name + ":" + type;
                first = false;
            }
            return Value::make_string(joined);
        }

        auto var_types = module.variables();
        auto found_var = var_types.find(constant_name);
        if (found_var == var_types.end()) {
            throw ModuleError(module_name + "." + constant_name + ": unknown module constant");
        }

        Value value;
        try {
            value = module.get_constant(constant_name);
        } catch (const ModuleError& error) {
            throw;
        }
        std::string expected_type = found_var->second;
        std::string actual_type = python_type_name(value);
        if (expected_type != actual_type) {
            throw ModuleError(module_name + "." + constant_name + ": expected " +
                              expected_type + ", received " + actual_type);
        }
        return value;
    }

private:
    std::map<std::string, std::shared_ptr<Module>> available_;
    std::set<std::string> loaded_;

    Module& get(const std::string& module_name) const {
        if (loaded_.count(module_name) == 0) {
            throw ModuleError(module_name + ": module is not included");
        }
        auto found = available_.find(module_name);
        if (found == available_.end()) {
            throw ModuleError(module_name + ": module is not included");
        }
        return *found->second;
    }

    static void validate_arguments(const std::string& module_name, const std::string& fn,
                                   const std::vector<Value>& args) {
        (void)module_name;
        (void)fn;
        for (const auto& argument : args) {
            bool usable = false;
            switch (argument.type) {
                case Value::Type::Bool:
                case Value::Type::Int:
                case Value::Type::Float:
                case Value::Type::String:
                    usable = true;
                    break;
                default:
                    usable = false;
                    break;
            }
            if (!usable) {
                throw ModuleError(
                    value_repr(argument) +
                    " is not a usable modules argument! Error code:\"UnknownModuleArgument\"");
            }
        }
    }

    static void validate_result(const std::string& module_name, const std::string& fn,
                                const Value& result) {
        switch (result.type) {
            case Value::Type::Bool:
            case Value::Type::Int:
            case Value::Type::Float:
            case Value::Type::String:
            case Value::Type::ModuleObject:
                return;
            default:
                break;
        }
        throw ModuleError(module_name + "." + fn + ": module returned an unsupported value");
    }

    static Value wrap_module_result(const std::string& module_name, Value result) {
        if (result.type == Value::Type::ModuleObject) {
            if (!result.module_value) {
                result.module_value = std::make_shared<ModuleObject>(module_name, nullptr);
            } else if (result.module_value->module_name.empty()) {
                result.module_value->module_name = module_name;
            }
        }
        return result;
    }

    static std::string python_type_name(const Value& value) {
        switch (value.type) {
            case Value::Type::Bool: return "bool";
            case Value::Type::Int: return "int";
            case Value::Type::Float: return "float";
            case Value::Type::String: return "string";
            default:
                throw ModuleError("unsupported module value type: " + value.type_name());
        }
    }
};

} // namespace resiris