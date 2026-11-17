#include "helpers.hpp"

using namespace resiris;

namespace {

resiris::Value run_value(const std::string& source, const std::string& name) {
    testutil::Interp app;
    app.exec(source);
    return testutil::variable(*app.interp, name);
}

} // namespace

TEST_CASE("declaration: int, float, string, bool", "[declarations]") {
    auto value = run_value("v a int = 5\n", "a");
    REQUIRE(value.type == Value::Type::Int);
    REQUIRE(value.int_value == 5);
    REQUIRE(value.type_name() == "int");

    value = run_value("v a float = 2.5\n", "a");
    REQUIRE(value.type == Value::Type::Float);
    REQUIRE(value.float_value == 2.5);

    value = run_value("v a string = \"hi\"\n", "a");
    REQUIRE(value.type == Value::Type::String);
    REQUIRE(value.string_value == "hi");

    value = run_value("v a bool = true\n", "a");
    REQUIRE(value.type == Value::Type::Bool);
    REQUIRE(value.bool_value == true);
}

TEST_CASE("declaration: a type is required", "[declarations]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v a = 5\n"),
                        Catch::Contains("a type name is required in a declaration"));
}

TEST_CASE("declaration: constant is flagged", "[declarations]") {
    testutil::Interp app;
    auto& variables = app.exec("c a int = 5\n");
    REQUIRE(variables.at("a").is_constant == true);
    REQUIRE(variables.at("a").value.int_value == 5);
}

TEST_CASE("declaration: value is required for non FunctionalObject", "[declarations]") {
    REQUIRE_NOTHROW(testutil::parse("v a int\n"));
    // A declaration without a value is a MissingValueError at runtime.
    REQUIRE_THROWS_AS(run_value("v a int\n", "a"), MissingValueError);
}

TEST_CASE("declaration: type coercion int to float", "[declarations]") {
    testutil::Interp app;
    auto& variables = app.exec("v a float = 5\n");
    REQUIRE(variables.at("a").value.type == Value::Type::Float);
    REQUIRE(variables.at("a").value.float_value == 5.0);
}

TEST_CASE("declaration: string to float is rejected", "[declarations]") {
    // Only int -> float is coerced implicitly; strings are never parsed.
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v a float = \"2.5\"\n"),
                        Catch::Contains("a float value is required, received: str"));
}

TEST_CASE("declaration: mismatched types are rejected", "[declarations]") {
    REQUIRE_THROWS_AS(run_value("v a int = \"text\"\n", "a"), ResirisTypeError);
    REQUIRE_THROWS_AS(run_value("v a int = true\n", "a"), ResirisTypeError);
}

TEST_CASE("assignment: simple reassignment", "[declarations]") {
    auto value = run_value("v a int = 1\na = 2\n", "a");
    REQUIRE(value.int_value == 2);
}

TEST_CASE("assignment: constant cannot be reassigned", "[declarations]") {
    testutil::Interp app;
    app.exec("c a int = 1\n");
    REQUIRE_THROWS_AS(app.exec("a = 2\n"), ConstantAssignmentError);
}

TEST_CASE("assignment: unknown variable", "[declarations]") {
    testutil::Interp app;
    REQUIRE_THROWS_AS(app.exec("a = 2\n"), UnknownVariableError);
}

TEST_CASE("assignment: compound operators", "[declarations]") {
    REQUIRE(run_value("v a int = 4\na += 1\n", "a").int_value == 5);
    REQUIRE(run_value("v a int = 4\na -= 1\n", "a").int_value == 3);
    REQUIRE(run_value("v a int = 4\na *= 2\n", "a").int_value == 8);
    REQUIRE(run_value("v a float = 4.0\na /= 2\n", "a").float_value == 2.0);
}

TEST_CASE("assignment: reassigning with a different type is rejected", "[declarations]") {
    testutil::Interp app;
    app.exec("v a int = 1\n");
    REQUIRE_THROWS_AS(app.exec("a = 1.5\n"), ResirisTypeError);
}

TEST_CASE("assignment: int compound division is rejected", "[declarations]") {
    // int /= int raises IntDivisionError, exactly like `int / int`.
    testutil::Interp app;
    app.exec("v a int = 8\n");
    REQUIRE_THROWS_AS(app.exec("a /= 2\n"), IntDivisionError);
}