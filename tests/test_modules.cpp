#include "helpers.hpp"

using namespace resiris;

namespace {

resiris::Value run_value(const std::string& source, const std::string& name) {
    testutil::Interp app;
    app.exec(source);
    return testutil::variable(*app.interp, name);
}

} // namespace

TEST_CASE("module: include and call", "[modules]") {
    auto value = run_value("<include> RSMath\nv x float = RSMath.sqrt(25)\n", "x");
    REQUIRE(value.float_value == 5.0);
}

TEST_CASE("module: constant access", "[modules]") {
    auto value = run_value("<include> RSMath\nv x float = RSMath[PI]\n", "x");
    REQUIRE(value.float_value == 3.141592653589793);
}

TEST_CASE("module: missing include is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v x float = RSMath.sqrt(25)\n"),
                        Catch::Contains("is not included"));
}

TEST_CASE("module: double include is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSMath\n<include> RSMath\n"),
                        Catch::Contains("SameModuleMultiCall"));
}

TEST_CASE("module: unknown module is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> NoSuchModule\n"),
                        Catch::Contains("MissingModule"));
}

TEST_CASE("module: unknown function is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSMath\nv x float = RSMath.nope(1)\n"),
                        Catch::Contains("unknown module function"));
}

TEST_CASE("module: unknown constant is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSMath\nv x float = RSMath[NOPE]\n"),
                        Catch::Contains("unknown module constant"));
}

TEST_CASE("module: a string argument is a signature error", "[modules]") {
    // Strings are primitives, so they reach the module and fail there.
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSMath\nv s string = \"x\"\nv y float = RSMath.sqrt(s)\n"),
                        Catch::Contains("invalid argument count or module function arguments"));
}

TEST_CASE("module: a module object argument is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("<include> RSMath, RSBase\n"
                 "v o ModuleObject = RSBase.await()\n"
                 "v y float = RSMath.sqrt(o)\n"),
        Catch::Contains("UnknownModuleArgument"));
}

TEST_CASE("module: wrong argument count is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("<include> RSMath\nv x float = RSMath.sqrt()\n"),
        Catch::Contains("invalid argument count or module function arguments"));
}

TEST_CASE("module: bare module access is rejected", "[modules]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("<include> RSMath\nv x float = RSMath.PI\n"),
        Catch::Contains("module `.` access is only valid for function calls"));
}

TEST_CASE("rsmath: numeric functions", "[modules]") {
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.abs(-3)\n", "x").float_value == 3.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.sqrt(16)\n", "x").float_value == 4.0);
    // cbrt matches the prototype bit for bit: math.cbrt(27) == 3.0000000000000004.
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.cbrt(27)\n", "x").float_value ==
            3.0000000000000004);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.pow(2, 10)\n", "x").float_value == 1024.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.floor(1.7)\n", "x").float_value == 1.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.ceil(1.2)\n", "x").float_value == 2.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.round(1.5)\n", "x").float_value == 2.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.ln(1)\n", "x").float_value == 0.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.log10(1000)\n", "x").float_value == 3.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.sin(0)\n", "x").float_value == 0.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.cos(0)\n", "x").float_value == 1.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.tan(0)\n", "x").float_value == 0.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.hypot(3, 4)\n", "x").float_value == 5.0);
}

TEST_CASE("rsmath: round is half to even", "[modules]") {
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.round(0.5)\n", "x").float_value == 0.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.round(1.5)\n", "x").float_value == 2.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.round(2.5)\n", "x").float_value == 2.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.round(-0.5)\n", "x").float_value == -0.0);
}

TEST_CASE("rsmath: integer helpers", "[modules]") {
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.factorial(5)\n", "x").float_value == 120.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.ncr(5, 2)\n", "x").float_value == 10.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.npr(5, 2)\n", "x").float_value == 20.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.gcd(12, 8)\n", "x").float_value == 4.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.lcm(4, 6)\n", "x").float_value == 12.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.mod(7, 3)\n", "x").float_value == 1.0);
}

TEST_CASE("rsmath: min, max, clamp", "[modules]") {
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.min(1, 2)\n", "x").float_value == 1.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.max(1, 2)\n", "x").float_value == 2.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.clamp(5, 0, 3)\n", "x").float_value == 3.0);
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.clamp(1, 0, 3)\n", "x").float_value == 1.0);
}

TEST_CASE("rsmath: pythagoras and predicates", "[modules]") {
    REQUIRE(run_value("<include> RSMath\nv x float = RSMath.pythagoras(3, 4)\n", "x").float_value == 5.0);
    REQUIRE(run_value("<include> RSMath\nv x bool = RSMath.is_nan(0.0)\n", "x").bool_value == false);
    REQUIRE(run_value("<include> RSMath\nv x bool = RSMath.is_finite(1.0)\n", "x").bool_value == true);
    REQUIRE(run_value("<include> RSMath\nv x bool = RSMath.is_finite(0.0)\n", "x").bool_value == true);
}

TEST_CASE("rsmath: a domain error surfaces as a Resiris error", "[modules]") {
    // The prototype lets a raw ValueError escape from the module; the C++ port
    // reports the same message through the module error type.
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("<include> RSMath\nv x float = RSMath.ln(0)\n"),
                        Catch::Contains("math domain error"));
}

TEST_CASE("rsmath: module metadata", "[modules]") {
    resiris::RsMathModule math;
    auto functions = math.function_names();
    REQUIRE(functions.front() == "abs");
    REQUIRE(functions.back() == "pythagoras");
    REQUIRE(functions.size() == 29);
    auto variables = math.variables();
    REQUIRE(variables.at("PI") == "float");
    REQUIRE(variables.at("E") == "float");
}

TEST_CASE("rsbase: module metadata", "[modules]") {
    resiris::RsBaseModule base;
    auto functions = base.function_names();
    REQUIRE(functions.size() == 7);
    REQUIRE(functions.front() == "await");
    REQUIRE(functions.back() == "process");
    REQUIRE(base.variables().empty());
}
