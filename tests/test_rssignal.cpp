#include "helpers.hpp"

using namespace resiris;

namespace {

Value call(RsSignalModule& module, const std::string& fn, const std::vector<Value>& args) {
    return module.call_function(fn, args);
}

} // namespace

TEST_CASE("rssignal: module metadata", "[rssignal]") {
    RsSignalModule module;
    REQUIRE(module.module_name() == "RSSignal");
    auto functions = module.function_names();
    REQUIRE(functions.size() == 6);
    REQUIRE(functions.front() == "set_signal_mode");
    REQUIRE(functions.back() == "get_analog_data");
    REQUIRE(module.variables().size() == 26);
    REQUIRE(module.variables().at("GENERAL_DIGITAL_GPIO4") == "int");
}

TEST_CASE("rssignal: constants", "[rssignal]") {
    RsSignalModule module;
    REQUIRE(module.get_constant("BOOTSTRAP_GPIO0").int_value == 0);
    REQUIRE(module.get_constant("GENERAL_DIGITAL_GPIO4").int_value == 4);
    REQUIRE(module.get_constant("ADC1_WIFI_SAFE_GPIO32").int_value == 32);
    REQUIRE(module.get_constant("ADC1_WIFI_SAFE_GPIO39").int_value == 39);
    REQUIRE_THROWS_AS(module.get_constant("GPIO99"), ModuleError);
}

TEST_CASE("rssignal: output lifecycle", "[rssignal]") {
    RsSignalModule module;
    REQUIRE(call(module, "get_signal_mode", {Value::make_int(4)}).bool_value == false);
    REQUIRE(call(module, "set_signal_mode",
                 {Value::make_int(4), Value::make_bool(true)}).bool_value == true);
    REQUIRE(call(module, "get_signal_mode", {Value::make_int(4)}).bool_value == true);
    REQUIRE(call(module, "get_digital_data", {Value::make_int(4)}).bool_value == false);
    REQUIRE(call(module, "send_signal", {Value::make_int(4)}).bool_value == true);
    REQUIRE(call(module, "get_digital_data", {Value::make_int(4)}).bool_value == true);
    REQUIRE(call(module, "stop_signal", {Value::make_int(4)}).bool_value == true);
    REQUIRE(call(module, "get_digital_data", {Value::make_int(4)}).bool_value == false);
}

TEST_CASE("rssignal: input-only pins", "[rssignal]") {
    RsSignalModule module;
    REQUIRE_THROWS_AS(call(module, "set_signal_mode",
                           {Value::make_int(34), Value::make_bool(true)}),
                      ResirisError);
    REQUIRE(call(module, "set_signal_mode",
                 {Value::make_int(34), Value::make_bool(false)}).bool_value == true);
}

TEST_CASE("rssignal: invalid operations", "[rssignal]") {
    RsSignalModule module;
    REQUIRE_THROWS_AS(call(module, "send_signal", {Value::make_int(4)}), ResirisError);
    REQUIRE_THROWS_AS(call(module, "send_signal", {Value::make_int(99)}), ResirisError);
    REQUIRE_THROWS_AS(call(module, "get_analog_data", {Value::make_int(4)}), ResirisError);
    REQUIRE(call(module, "get_analog_data", {Value::make_int(32)}).float_value == 0.0);
}

TEST_CASE("rssignal: interpreter integration", "[rssignal]") {
    testutil::Interp app;
    app.exec("<include> RSSignal\n"
             "v pin int = RSSignal[GENERAL_DIGITAL_GPIO4]\n"
             "RSSignal.set_signal_mode(pin, true)\n"
             "RSSignal.send_signal(pin)\n"
             "v high bool = RSSignal.get_digital_data(pin)\n");
    REQUIRE(testutil::variable(*app.interp, "pin").int_value == 4);
    REQUIRE(testutil::variable(*app.interp, "high").bool_value == true);
}
