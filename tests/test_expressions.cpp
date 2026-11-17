#include "helpers.hpp"

using namespace resiris;

namespace {

resiris::Value run_value(const std::string& source, const std::string& name) {
    testutil::Interp app;
    app.exec(source);
    return testutil::variable(*app.interp, name);
}

const std::string kIntDivisionMessage =
    "Cant division with int type! Must use float! Error code:\"IntDivisionError\"";

} // namespace

TEST_CASE("arithmetic: modulo follows Python floor semantics", "[expressions]") {
    REQUIRE(run_value("v x int = -7 % 3\n", "x").int_value == 2);
    REQUIRE(run_value("v x int = 7 % -3\n", "x").int_value == -2);
    REQUIRE(run_value("v x int = -7 % -3\n", "x").int_value == -1);
    REQUIRE(run_value("v x int = 7 % 3\n", "x").int_value == 1);
    REQUIRE(run_value("v x float = -7.5 % 2.0\n", "x").float_value == 0.5);
    REQUIRE(run_value("v x float = 7.5 % -2.0\n", "x").float_value == -0.5);
}

TEST_CASE("arithmetic: mixed int and float keeps float", "[expressions]") {
    auto value = run_value("v x float = 2 * 3.5\n", "x");
    REQUIRE(value.type == Value::Type::Float);
    REQUIRE(value.float_value == 7.0);
}

TEST_CASE("arithmetic: large integers", "[expressions]") {
    REQUIRE(run_value("v x int = 123456789012345678\n", "x").int_value ==
            123456789012345678LL);
}

TEST_CASE("formatting: value_to_string matches Python str", "[expressions]") {
    REQUIRE(value_to_string(Value::make_float(1.0)) == "1.0");
    REQUIRE(value_to_string(Value::make_float(-0.0)) == "-0.0");
    REQUIRE(value_to_string(Value::make_float(0.1 + 0.2)) == "0.30000000000000004");
    REQUIRE(value_to_string(Value::make_float(1.0 / 3.0)) == "0.3333333333333333");
    REQUIRE(value_to_string(Value::make_float(5.0 / 2.0)) == "2.5");
    REQUIRE(value_to_string(Value::make_int(42)) == "42");
    REQUIRE(value_to_string(Value::make_bool(true)) == "True");
    REQUIRE(value_to_string(Value::make_string("text")) == "text");
}

TEST_CASE("arithmetic: ints", "[expressions]") {    REQUIRE(run_value("v x int = 1 + 2\n", "x").int_value == 3);
    REQUIRE(run_value("v x int = 5 - 2\n", "x").int_value == 3);
    REQUIRE(run_value("v x int = 3 * 4\n", "x").int_value == 12);
    REQUIRE(run_value("v x int = 7 % 3\n", "x").int_value == 1);
}

TEST_CASE("arithmetic: floats", "[expressions]") {
    REQUIRE(run_value("v x float = 1.5 + 2.5\n", "x").float_value == 4.0);
    REQUIRE(run_value("v x float = 5.5 - 2.0\n", "x").float_value == 3.5);
    REQUIRE(run_value("v x float = 2.5 * 4\n", "x").float_value == 10.0);
    REQUIRE(run_value("v x float = 7.5 % 2.0\n", "x").float_value == 1.5);
}

TEST_CASE("division: float results are valid", "[expressions]") {
    REQUIRE(run_value("v x float = 10.0 / 2\n", "x").float_value == 5.0);
    REQUIRE(run_value("v x float = 10.0 / 2.0\n", "x").float_value == 5.0);
}

TEST_CASE("division: int operands are rejected", "[expressions]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v x float = 10 / 2\n"), Catch::Contains(kIntDivisionMessage));
    REQUIRE_THROWS_WITH(app.exec("v x float = 10 / 2.0\n"), Catch::Contains(kIntDivisionMessage));
    REQUIRE_THROWS_WITH(app.exec("v x int = 10\nx /= 2\n"), Catch::Contains(kIntDivisionMessage));
}

TEST_CASE("division: float compound division is valid", "[expressions]") {
    REQUIRE(run_value("v x float = 10.0\nx /= 2\n", "x").float_value == 5.0);
    REQUIRE(run_value("v x float = 10.0\nx /= 2.0\n", "x").float_value == 5.0);
}

TEST_CASE("division: by zero", "[expressions]") {
    {
        testutil::Interp app;
        REQUIRE_THROWS_WITH(app.exec("v x float = 1.0 / 0\n"), Catch::Contains("division by zero"));
    }
    {
        testutil::Interp app;
        REQUIRE_THROWS_WITH(app.exec("v x float = 1.0 / 0.0\n"), Catch::Contains("division by zero"));
    }
    {
        testutil::Interp app;
        REQUIRE_THROWS_WITH(app.exec("v x float = 1.0\nx /= 0\n"),
                            Catch::Contains("division by zero"));
    }
    {
        testutil::Interp app;
        REQUIRE_THROWS_WITH(app.exec("v x float = 1.0 % 0\n"), Catch::Contains("modulo by zero"));
    }
}

TEST_CASE("precedence: multiplication before addition", "[expressions]") {
    REQUIRE(run_value("v x int = 2 + 3 * 4\n", "x").int_value == 14);
    REQUIRE(run_value("v x int = (2 + 3) * 4\n", "x").int_value == 20);
    REQUIRE(run_value("v x int = 2 * 3 + 4 * 5\n", "x").int_value == 26);
}

TEST_CASE("precedence: unary minus", "[expressions]") {
    REQUIRE(run_value("v x int = -5\n", "x").int_value == -5);
    REQUIRE(run_value("v x int = -2 + 1\n", "x").int_value == -1);
    REQUIRE(run_value("v x int = 2 * -3\n", "x").int_value == -6);
    REQUIRE(run_value("v x float = -2.5\n", "x").float_value == -2.5);
}

TEST_CASE("comparisons: numeric", "[expressions]") {
    REQUIRE(run_value("v b bool = 1 < 2\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 2 <= 2\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 3 > 2\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 3 >= 4\n", "b").bool_value == false);
    REQUIRE(run_value("v b bool = 1 == 1\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 1 != 2\n", "b").bool_value == true);
}

TEST_CASE("comparisons: cross int/float equality", "[expressions]") {
    REQUIRE(run_value("v b bool = 1 == 1.0\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 1.5 == 1.5\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = 1.0 < 2\n", "b").bool_value == true);
}

TEST_CASE("comparisons: strings", "[expressions]") {
    REQUIRE(run_value("v b bool = \"a\" == \"a\"\n", "b").bool_value == true);
    REQUIRE(run_value("v b bool = \"a\" != \"b\"\n", "b").bool_value == true);
}

TEST_CASE("conversions: type() int to float", "[expressions]") {
    auto value = run_value("v x int = 10\nv y float = x.type(float)\n", "y");
    REQUIRE(value.type == Value::Type::Float);
    REQUIRE(value.float_value == 10.0);
}

TEST_CASE("conversions: type() float to int uses half-up rounding", "[expressions]") {
    REQUIRE(run_value("v a float = 0.4\nv r int = a.type(int)\n", "r").int_value == 0);
    REQUIRE(run_value("v a float = 0.5\nv r int = a.type(int)\n", "r").int_value == 1);
    REQUIRE(run_value("v a float = 1.4\nv r int = a.type(int)\n", "r").int_value == 1);
    REQUIRE(run_value("v a float = 1.5\nv r int = a.type(int)\n", "r").int_value == 2);
}

TEST_CASE("conversions: type() numeric to string", "[expressions]") {
    REQUIRE(run_value("v a int = 10\nv r string = a.type(string)\n", "r").string_value == "10");
    REQUIRE(run_value("v a float = 2.5\nv r string = a.type(string)\n", "r").string_value == "2.5");
}

TEST_CASE("conversions: type() bool conversions", "[expressions]") {
    REQUIRE(run_value("v a bool = true\nv r int = a.type(int)\n", "r").int_value == 1);
    REQUIRE(run_value("v a bool = false\nv r int = a.type(int)\n", "r").int_value == 0);
    REQUIRE(run_value("v a bool = true\nv r float = a.type(float)\n", "r").float_value == 1.0);
    REQUIRE(run_value("v a bool = false\nv r float = a.type(float)\n", "r").float_value == 0.0);
    REQUIRE(run_value("v a bool = true\nv r string = a.type(string)\n", "r").string_value == "true");
    REQUIRE(run_value("v a bool = false\nv r string = a.type(string)\n", "r").string_value == "false");
}

TEST_CASE("conversions: string to numeric is rejected", "[expressions]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v x string = \"10\"\nv y int = x.type(int)\n"),
                        Catch::Contains("ConversionFail"));
}

TEST_CASE("type(): returns the type name", "[expressions]") {
    REQUIRE(run_value("v a int = 1\nv b string = a.type()\n", "b").string_value == "int");
    REQUIRE(run_value("v a float = 1.0\nv b string = a.type()\n", "b").string_value == "float");
    REQUIRE(run_value("v a string = \"s\"\nv b string = a.type()\n", "b").string_value == "string");
    REQUIRE(run_value("v a bool = true\nv b string = a.type()\n", "b").string_value == "bool");
}

TEST_CASE("string(): renders the value", "[expressions]") {
    // `.string()` is a type conversion, so a bool renders lowercase.
    REQUIRE(run_value("v a int = 1\nv b string = a.string()\n", "b").string_value == "1");
    REQUIRE(run_value("v a float = 2.5\nv b string = a.string()\n", "b").string_value == "2.5");
    REQUIRE(run_value("v a bool = true\nv b string = a.string()\n", "b").string_value == "true");
    REQUIRE(run_value("v a bool = false\nv b string = a.string()\n", "b").string_value == "false");
}

TEST_CASE("string_conversion: string(bool) is lowercase", "[expressions]") {
    REQUIRE(run_value("v a bool = true\nv b string = a.type(string)\n", "b").string_value == "true");
}

TEST_CASE("print_cmd: prints the evaluated value", "[expressions][print]") {
    testutil::CaptureStdout capture;
    {
        testutil::Interp app;
        app.exec("v x int = 42\nprint_cmd(x)\n");
    }
    REQUIRE(capture.str() == "42\n");
}

TEST_CASE("print_cmd: prints strings", "[expressions][print]") {
    testutil::CaptureStdout capture;
    {
        testutil::Interp app;
        app.exec("print_cmd(\"hello\")\n");
    }
    REQUIRE(capture.str() == "hello\n");
}

TEST_CASE("print_cmd: prints float formatting", "[expressions][print]") {
    testutil::CaptureStdout capture;
    {
        testutil::Interp app;
        app.exec("v a float = 1.0\nprint_cmd(a)\n");
    }
    REQUIRE(capture.str() == "1.0\n");
}

TEST_CASE("print_cmd: prints bool as Python str", "[expressions][print]") {
    testutil::CaptureStdout capture;
    {
        testutil::Interp app;
        app.exec("print_cmd(true)\nprint_cmd(false)\n");
    }
    REQUIRE(capture.str() == "True\nFalse\n");
}
