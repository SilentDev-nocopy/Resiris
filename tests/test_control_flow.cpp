#include "helpers.hpp"

using namespace resiris;

namespace {

std::string run_printing(const std::string& source) {
    testutil::CaptureStdout capture;
    {
        testutil::Interp app;
        app.exec(source);
    }
    return capture.str();
}

resiris::Value run_value(const std::string& source, const std::string& name) {
    testutil::Interp app;
    app.exec(source);
    return testutil::variable(*app.interp, name);
}

} // namespace

TEST_CASE("if: branches", "[control]") {
    REQUIRE(run_printing("v a int = 1\nif a == 1:\n\tprint_cmd(\"one\")\n") == "one\n");
    REQUIRE(run_printing("v a int = 2\nif a == 1:\n\tprint_cmd(\"one\")\nelse:\n\tprint_cmd(\"other\")\n") ==
            "other\n");
}

TEST_CASE("if: elif chain", "[control]") {
    std::string chain =
        "if a == 1:\n"
        "\tprint_cmd(\"one\")\n"
        "elif a == 2:\n"
        "\tprint_cmd(\"two\")\n"
        "elif a == 3:\n"
        "\tprint_cmd(\"three\")\n"
        "else:\n"
        "\tprint_cmd(\"other\")\n";
    REQUIRE(run_printing("v a int = 1\n" + chain) == "one\n");
    REQUIRE(run_printing("v a int = 2\n" + chain) == "two\n");
    REQUIRE(run_printing("v a int = 3\n" + chain) == "three\n");
    REQUIRE(run_printing("v a int = 4\n" + chain) == "other\n");
}

TEST_CASE("if: nesting", "[control]") {
    std::string source =
        "v a int = 2\n"
        "v b int = 2\n"
        "if a == 2:\n"
        "\tif b == 2:\n"
        "\t\tprint_cmd(\"case\")\n"
        "print_cmd(\"after\")\n";
    REQUIRE(run_printing(source) == "case\nafter\n");
}

TEST_CASE("function: parameters and return", "[control]") {
    auto value = run_value(
        "fn add(a, b):\n"
        "\treturn a + b\n"
        "v x int = add(2, 3)\n",
        "x");
    REQUIRE(value.int_value == 5);
}

TEST_CASE("function: return without a value", "[control]") {
    REQUIRE(run_printing(
                "fn doit():\n"
                "\tprint_cmd(\"in\")\n"
                "\treturn\n"
                "doit()\n"
                "print_cmd(\"out\")\n") == "in\nout\n");
}

TEST_CASE("function: recursion works", "[control]") {
    auto value = run_value(
        "fn fact(n):\n"
        "\tif n <= 1:\n"
        "\t\treturn 1\n"
        "\treturn n * fact(n - 1)\n"
        "v x int = fact(5)\n",
        "x");
    REQUIRE(value.int_value == 120);
}

TEST_CASE("function: duplicate name is rejected", "[control]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("fn a():\n\tpass\nfn a():\n\tpass\n"),
                        Catch::Contains("the function already exists"));
}

TEST_CASE("function: name collides with a variable", "[control]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v a int = 1\nfn a():\n\tpass\n"),
                        Catch::Contains("the name is already used as a variable"));
}

TEST_CASE("functional object: declaration and call", "[control]") {
    // Parameters are untyped names, exactly like the prototype.
    auto value = run_value(
        "v doubler FunctionalObject = FunctionalObject.new(number):\n"
        "\treturn number * 2\n"
        "v x int = doubler(21)\n",
        "x");
    REQUIRE(value.int_value == 42);
}

TEST_CASE("functional object: a callback parameter is not supported", "[control]") {
    // The prototype cannot infer a type for a FunctionalObject parameter, so a
    // FunctionalObject can only be called directly.
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("v doubler FunctionalObject = FunctionalObject.new(number):\n"
                 "\treturn number * 2\n"
                 "fn apply(callback, a):\n"
                 "\treturn callback(a)\n"
                 "v x int = apply(doubler, 5)\n"),
        Catch::Contains("type cannot be determined: FunctionalObject"));
}

TEST_CASE("functional object: argument count is checked", "[control]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(
        app.exec("v doubler FunctionalObject = FunctionalObject.new(number):\n"
                 "\treturn number * 2\n"
                 "v x int = doubler(1, 2)\n"),
        Catch::Contains("parameters required"));
}

TEST_CASE("mat: value cases", "[control]") {
    std::string body =
        "mat a:\n"
        "\t1:\n"
        "\t\tprint_cmd(\"one\")\n"
        "\t2:\n"
        "\t\tprint_cmd(\"two\")\n"
        "\telse:\n"
        "\t\tprint_cmd(\"other\")\n";
    REQUIRE(run_printing("v a int = 1\n" + body) == "one\n");
    REQUIRE(run_printing("v a int = 2\n" + body) == "two\n");
    REQUIRE(run_printing("v a int = 9\n" + body) == "other\n");
}

TEST_CASE("mat: string cases", "[control]") {
    std::string source =
        "v a string = \"b\"\n"
        "mat a:\n"
        "\t\"a\":\n"
        "\t\tprint_cmd(\"first\")\n"
        "\t\"b\":\n"
        "\t\tprint_cmd(\"second\")\n";
    REQUIRE(run_printing(source) == "second\n");
}

TEST_CASE("mat: type() cases", "[control]") {
    // A literal cannot be followed by `.`, so the checked value is a name.
    std::string chain =
        "mat b.type():\n"
        "\tint:\n"
        "\t\tprint_cmd(\"int\")\n"
        "\tfloat:\n"
        "\t\tprint_cmd(\"float\")\n"
        "\tstring:\n"
        "\t\tprint_cmd(\"string\")\n"
        "\tbool:\n"
        "\t\tprint_cmd(\"bool\")\n"
        "\telse:\n"
        "\t\tprint_cmd(\"other\")\n";
    REQUIRE(run_printing("v b int = 1\n" + chain) == "int\n");
    REQUIRE(run_printing("v b float = 1.0\n" + chain) == "float\n");
    REQUIRE(run_printing("v b string = \"s\"\n" + chain) == "string\n");
    REQUIRE(run_printing("v b bool = true\n" + chain) == "bool\n");
}

TEST_CASE("mat: runs every statement of the matched case", "[control]") {
    REQUIRE(run_printing(
                "v a int = 1\n"
                "mat a:\n"
                "\t1:\n"
                "\t\tprint_cmd(\"one\")\n"
                "\t\tprint_cmd(\"two\")\n") == "one\ntwo\n");
}

TEST_CASE("mat: body error is wrapped", "[control]") {
    testutil::Interp app;
    REQUIRE_THROWS_WITH(app.exec("v a int = 1\nmat a:\n\t1:\n\t\treturn 1 + \"x\"\n"),
                        Catch::Contains("MatchCaseExecutionError"));
}

// Regression: the scope holding a declaration target must be looked up after
// the initialiser is evaluated. Calling a function from the initialiser pushes
// a new scope, which can reallocate the scope stack the reference pointed into.
TEST_CASE("declaration inside a function body survives a call in its initialiser",
          "[control][scope]") {
    REQUIRE(run_printing(
                "fn inner():\n"
                "\treturn 5\n"
                "fn outer():\n"
                "\tv x int = inner()\n"
                "\treturn x + 10\n"
                "v result int = outer()\n"
                "print_cmd(result)\n") == "15\n");
}

TEST_CASE("assignment to a local survives a call in its right hand side",
          "[control][scope]") {
    REQUIRE(run_printing(
                "fn inner():\n"
                "\treturn 1\n"
                "fn outer(n):\n"
                "\tn = n + inner()\n"
                "\treturn n\n"
                "print_cmd(outer(5))\n") == "6\n");
}

TEST_CASE("a local declaration is visible to every later statement in the block",
          "[control][scope]") {
    REQUIRE(run_printing(
                "fn inner():\n"
                "\treturn 2\n"
                "fn outer():\n"
                "\tv a int = inner()\n"
                "\tv b int = a * 3\n"
                "\ta = b + 1\n"
                "\treturn a + b\n"
                "print_cmd(outer())\n") == "13\n");
}
