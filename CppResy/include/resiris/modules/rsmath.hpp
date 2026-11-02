#pragma once

#include "resiris/module.hpp"

namespace resiris {

class RsMathModule : public Module {
public:
    std::string module_name() const override { return "RSMath"; }

    std::vector<std::string> function_names() const override {
        return {
            "abs",       "sqrt",    "cbrt",   "pow",    "floor", "ceil",
            "round",     "ln",      "log10",  "sin",    "cos",   "tan",
            "asin",      "acos",    "atan",   "hypot",  "factorial", "ncr",
            "npr",       "gcd",     "lcm",    "mod",    "min",   "max",
            "clamp",     "is_nan",  "is_inf", "is_finite", "pythagoras",
        };
    }

    std::map<std::string, std::string> variables() const override {
        return {{"PI", "float"}, {"E", "float"}};
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string&, const std::shared_ptr<FrameAwareState>&,
                             const std::vector<Value>&) override {
        throw ModuleError("RSMath has no object methods");
    }
    Value get_constant(const std::string& name) const override;
};

} // namespace resiris