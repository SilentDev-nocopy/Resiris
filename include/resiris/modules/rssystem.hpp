#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "resiris/module.hpp"

namespace resiris {

// Facts about the machine the program is running on. The portable functions
// (cpu_cores, cpu_load, uptime, modules, report) work on every target. The
// hardware-only functions report the ESP32's own state and refuse to answer on
// a PC instead of returning fabricated values.
//
// uptime and cpu_load come from the host-installed program clock in
// resiris/platform.hpp. The registry is supplied through on_registered(), so
// modules() includes RSSystem itself without a hand-maintained name list.
class RsSystemModule : public Module {
public:
    std::string module_name() const override { return "RSSystem"; }

    std::vector<std::string> function_names() const override {
        return {"chip_model",   "chip_revision", "cpu_cores",
                "cpu_speed",    "cpu_load",      "memory_total",
                "memory_free",  "memory_used",   "memory_min_free",
                "psram_size",   "flash_total",   "flash_used",
                "uptime",       "modules",       "report"};
    }

    std::map<std::string, std::string> variables() const override { return {}; }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;

    Value call_object_method(const std::string& fn,
                             const std::shared_ptr<FrameAwareState>& handle,
                             const std::vector<Value>& args) override;

    Value get_constant(const std::string& name) const override;

    void on_registered(ModuleRegistry& registry) override;

private:
    std::string module_list() const;
    std::string build_report() const;

    const ModuleRegistry* registry_ = nullptr;
};

} // namespace resiris
