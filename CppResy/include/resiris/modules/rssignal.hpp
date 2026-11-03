#pragma once

#include <map>

#include "resiris/module.hpp"

namespace resiris {

// GPIO module for the ESP32-WROOM-32 pin map.
//
// The desktop interpreter has no ESP32 GPIO peripheral, so this build keeps a
// simulated pin state in memory: outputs can be driven HIGH/LOW and read back,
// inputs default to LOW, and analog reads return 0.0. The public API matches
// the planned hardware module so a firmware backend can replace the state
// handling without changing Resiris programs.
class RsSignalModule : public Module {
public:
    std::string module_name() const override { return "RSSignal"; }

    std::vector<std::string> function_names() const override {
        return {"set_signal_mode", "send_signal", "stop_signal",
                "get_signal_mode", "get_digital_data", "get_analog_data"};
    }

    std::map<std::string, std::string> variables() const override {
        return {
            {"BOOTSTRAP_GPIO0", "int"},
            {"UART0_GPIO1", "int"},
            {"BOOTSTRAP_GPIO2", "int"},
            {"UART0_GPIO3", "int"},
            {"GENERAL_DIGITAL_GPIO4", "int"},
            {"BOOTSTRAP_GPIO5", "int"},
            {"BOOTSTRAP_GPIO12", "int"},
            {"GENERAL_DIGITAL_GPIO13", "int"},
            {"GENERAL_DIGITAL_GPIO14", "int"},
            {"BOOTSTRAP_GPIO15", "int"},
            {"GENERAL_DIGITAL_GPIO16", "int"},
            {"GENERAL_DIGITAL_GPIO17", "int"},
            {"GENERAL_DIGITAL_GPIO18", "int"},
            {"GENERAL_DIGITAL_GPIO19", "int"},
            {"GENERAL_DIGITAL_GPIO21", "int"},
            {"GENERAL_DIGITAL_GPIO22", "int"},
            {"GENERAL_DIGITAL_GPIO23", "int"},
            {"GENERAL_DIGITAL_GPIO25", "int"},
            {"GENERAL_DIGITAL_GPIO26", "int"},
            {"GENERAL_DIGITAL_GPIO27", "int"},
            {"ADC1_WIFI_SAFE_GPIO32", "int"},
            {"ADC1_WIFI_SAFE_GPIO33", "int"},
            {"ADC1_WIFI_SAFE_GPIO34", "int"},
            {"ADC1_WIFI_SAFE_GPIO35", "int"},
            {"ADC1_WIFI_SAFE_GPIO36", "int"},
            {"ADC1_WIFI_SAFE_GPIO39", "int"},
        };
    }

    Value call_function(const std::string& fn, const std::vector<Value>& args) override;
    Value call_object_method(const std::string&, const std::shared_ptr<FrameAwareState>&,
                             const std::vector<Value>&) override {
        throw ModuleError("RSSignal has no object methods");
    }
    Value get_constant(const std::string& name) const override;

private:
    std::map<int, bool> output_mode_;
    std::map<int, bool> digital_level_;
};

} // namespace resiris
