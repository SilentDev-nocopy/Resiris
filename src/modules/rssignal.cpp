#include "resiris/modules/rssignal.hpp"

#include <array>
#include <string>

namespace resiris {

namespace {

void require_count(const std::vector<Value>& args, std::size_t count) {
    if (args.size() != count) {
        throw ModuleSignatureError();
    }
}

Int require_pin(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size() || args[index].type != Value::Type::Int) {
        throw ModuleSignatureError();
    }
    return args[index].int_value;
}

bool require_bool(const std::vector<Value>& args, std::size_t index) {
    if (index >= args.size() || args[index].type != Value::Type::Bool) {
        throw ModuleSignatureError();
    }
    return args[index].bool_value;
}

bool is_known_pin(Int pin) {
    static const std::array<Int, 26> pins = {{
        0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18,
        19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39,
    }};
    for (Int candidate : pins) {
        if (candidate == pin) {
            return true;
        }
    }
    return false;
}

bool is_input_only(Int pin) {
    return pin == 34 || pin == 35 || pin == 36 || pin == 39;
}

bool is_adc_pin(Int pin) {
    return pin == 32 || pin == 33 || pin == 34 || pin == 35 || pin == 36 || pin == 39;
}

void require_known_pin(const std::string& fn, Int pin) {
    if (!is_known_pin(pin)) {
        throw ResirisError("RSSignal." + fn + ": unknown GPIO pin: " + std::to_string(pin));
    }
}

} // namespace

Value RsSignalModule::call_function(const std::string& fn, const std::vector<Value>& args) {
    if (fn == "set_signal_mode") {
        require_count(args, 2);
        Int pin = require_pin(args, 0);
        bool output = require_bool(args, 1);
        require_known_pin(fn, pin);
        if (output && is_input_only(pin)) {
            throw ResirisError("RSSignal." + fn + ": GPIO" + std::to_string(pin) +
                               " is input-only");
        }
        output_mode_[pin] = output;
        if (!output) {
            digital_level_[pin] = false;
        }
        return Value::make_bool(true);
    }
    if (fn == "send_signal" || fn == "stop_signal") {
        require_count(args, 1);
        Int pin = require_pin(args, 0);
        require_known_pin(fn, pin);
        if (!output_mode_[pin]) {
            throw ResirisError("RSSignal." + fn + ": GPIO" + std::to_string(pin) +
                               " is not configured as an output");
        }
        digital_level_[pin] = fn == "send_signal";
        return Value::make_bool(true);
    }
    if (fn == "get_signal_mode") {
        require_count(args, 1);
        Int pin = require_pin(args, 0);
        require_known_pin(fn, pin);
        return Value::make_bool(output_mode_[pin]);
    }
    if (fn == "get_digital_data") {
        require_count(args, 1);
        Int pin = require_pin(args, 0);
        require_known_pin(fn, pin);
        return Value::make_bool(digital_level_[pin]);
    }
    if (fn == "get_analog_data") {
        require_count(args, 1);
        Int pin = require_pin(args, 0);
        require_known_pin(fn, pin);
        if (!is_adc_pin(pin)) {
            throw ResirisError("RSSignal." + fn + ": GPIO" + std::to_string(pin) +
                               " is not an ADC pin");
        }
        // No ADC hardware is present on the desktop. The simulated input is 0.
        return Value::make_float(0.0);
    }

    throw ModuleError("RSSignal." + fn + ": unknown module function");
}

Value RsSignalModule::get_constant(const std::string& name) const {
    if (name == "BOOTSTRAP_GPIO0") return Value::make_int(0);
    if (name == "UART0_GPIO1") return Value::make_int(1);
    if (name == "BOOTSTRAP_GPIO2") return Value::make_int(2);
    if (name == "UART0_GPIO3") return Value::make_int(3);
    if (name == "GENERAL_DIGITAL_GPIO4") return Value::make_int(4);
    if (name == "BOOTSTRAP_GPIO5") return Value::make_int(5);
    if (name == "BOOTSTRAP_GPIO12") return Value::make_int(12);
    if (name == "GENERAL_DIGITAL_GPIO13") return Value::make_int(13);
    if (name == "GENERAL_DIGITAL_GPIO14") return Value::make_int(14);
    if (name == "BOOTSTRAP_GPIO15") return Value::make_int(15);
    if (name == "GENERAL_DIGITAL_GPIO16") return Value::make_int(16);
    if (name == "GENERAL_DIGITAL_GPIO17") return Value::make_int(17);
    if (name == "GENERAL_DIGITAL_GPIO18") return Value::make_int(18);
    if (name == "GENERAL_DIGITAL_GPIO19") return Value::make_int(19);
    if (name == "GENERAL_DIGITAL_GPIO21") return Value::make_int(21);
    if (name == "GENERAL_DIGITAL_GPIO22") return Value::make_int(22);
    if (name == "GENERAL_DIGITAL_GPIO23") return Value::make_int(23);
    if (name == "GENERAL_DIGITAL_GPIO25") return Value::make_int(25);
    if (name == "GENERAL_DIGITAL_GPIO26") return Value::make_int(26);
    if (name == "GENERAL_DIGITAL_GPIO27") return Value::make_int(27);
    if (name == "ADC1_WIFI_SAFE_GPIO32") return Value::make_int(32);
    if (name == "ADC1_WIFI_SAFE_GPIO33") return Value::make_int(33);
    if (name == "ADC1_WIFI_SAFE_GPIO34") return Value::make_int(34);
    if (name == "ADC1_WIFI_SAFE_GPIO35") return Value::make_int(35);
    if (name == "ADC1_WIFI_SAFE_GPIO36") return Value::make_int(36);
    if (name == "ADC1_WIFI_SAFE_GPIO39") return Value::make_int(39);
    throw ModuleError("RSSignal." + name + ": unknown module constant");
}

} // namespace resiris
