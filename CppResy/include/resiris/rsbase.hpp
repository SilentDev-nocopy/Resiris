#pragma once

#include "resiris/module.hpp"

namespace resiris {

// A timer handle is module-owned state. The interpreter is the time source:
// it injects `now` before every object method call and fires `fire_callback`
// (a Resiris function name) once after the call that set it.
struct TimerState : FrameAwareState {
    bool running = false;
    double start = 0.0;
    double duration = 0.0;
    std::optional<std::string> callback;
    bool fired = false;
    bool freed = false;
};

class RsBaseModule : public Module {
public:
    std::string module_name() const override { return "RSBase"; }

    std::vector<std::string> function_names() const override {
        return {"await", "set_timer", "on_timeout", "reset_timer",
                "stop_timer", "free_timer", "process"};
    }

    std::map<std::string, std::string> variables() const override { return {}; }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string& fn, const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;
    Value get_constant(const std::string& name) const override;
};

} // namespace resiris