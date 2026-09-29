#include "resiris/rsbase.hpp"

#include <cstdlib>

namespace resiris {

namespace {

// Python float(value) semantics: ints and floats pass through, strings parse.
double to_float(const Value& v) {
    switch (v.type) {
        case Value::Type::Int: return static_cast<double>(v.int_value);
        case Value::Type::Bool: return v.bool_value ? 1.0 : 0.0;
        case Value::Type::Float: return v.float_value;
        case Value::Type::String: {
            char* end = nullptr;
            const std::string& s = v.string_value;
            const char* cstr = s.c_str();
            double result = std::strtod(cstr, &end);
            if (end == cstr) {
                throw ModuleSignatureError();
            }
            return result;
        }
        default: throw ModuleSignatureError();
    }
}

std::shared_ptr<TimerState> require_handle(const std::shared_ptr<FrameAwareState>& handle) {
    if (!handle) {
        throw ModuleError("module object handle is missing");
    }
    auto state = std::dynamic_pointer_cast<TimerState>(handle);
    if (!state) {
        throw ModuleError("RSBase method requires a timer handle");
    }
    return state;
}

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

const Value& require_string(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size()) {
        throw ModuleSignatureError();
    }
    if (args[index].type != Value::Type::String) {
        throw ModuleSignatureError();
    }
    return args[index];
}

} // namespace

Value RsBaseModule::call_function(const std::string& fn, const std::vector<Value>& args) {
    if (fn != "await") {
        throw ModuleError("RSBase." + fn + ": unknown module function");
    }
    require_count(args, 0);

    auto timer = std::make_shared<TimerState>();
    return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
}

Value RsBaseModule::call_object_method(const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
                                       const std::vector<Value>& args) {
    auto timer = require_handle(handle);

    if (fn == "set_timer") {
        require_count(args, 1);
        timer->duration = to_float(args[0]);
        timer->start = timer->now;
        timer->running = true;
        timer->fired = false;
        timer->freed = false;
        return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
    }

    if (fn == "on_timeout") {
        require_count(args, 1);
        timer->callback = require_string(args, 0).string_value;
        return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
    }

    if (fn == "reset_timer") {
        require_count(args, 0);
        timer->start = timer->now;
        timer->running = true;
        timer->fired = false;
        timer->freed = false;
        return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
    }

    if (fn == "stop_timer") {
        require_count(args, 0);
        timer->running = false;
        return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
    }

    if (fn == "free_timer") {
        require_count(args, 0);
        timer->running = false;
        timer->freed = true;
        return Value::make_module(std::make_shared<ModuleObject>("RSBase", timer));
    }

    if (fn == "process") {
        require_count(args, 0);

        if (!timer->running) {
            return Value::make_float(0.0);
        }

        double remaining = timer->start + timer->duration - timer->now;
        if (remaining > 0.0) {
            return Value::make_float(remaining);
        }

        if (!timer->fired) {
            timer->fired = true;
            if (timer->callback) {
                timer->fire_callback = timer->callback;
            }
        }

        return Value::make_float(0.0);
    }

    throw ModuleError("RSBase." + fn + ": unknown module function");
}

Value RsBaseModule::get_constant(const std::string& name) const {
    throw ModuleError("RSBase." + name + ": unknown module constant");
}

} // namespace resiris