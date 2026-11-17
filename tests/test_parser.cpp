#include "helpers.hpp"

using namespace resiris;

TEST_CASE("parser: empty program", "[parser]") {
    auto program = testutil::parse("\n\n");
    REQUIRE(program.statements.empty());
}

TEST_CASE("parser: include statement", "[parser]") {
    auto program = testutil::parse("<include> RSMath, RSBase\n");
    REQUIRE(program.statements.size() == 1);
    auto include = std::static_pointer_cast<Include>(program.statements[0]);
    REQUIRE(include->modules.size() == 2);
    REQUIRE(include->modules[0] == "RSMath");
    REQUIRE(include->modules[1] == "RSBase");
}

TEST_CASE("parser: declaration without value", "[parser]") {
    auto program = testutil::parse("v a int\n");
    auto declaration = std::static_pointer_cast<Declaration>(program.statements[0]);
    REQUIRE(declaration->name == "a");
    REQUIRE(declaration->type_name == "int");
    REQUIRE_FALSE(declaration->value);
}

TEST_CASE("parser: function definition", "[parser]") {
    auto program = testutil::parse("fn add(a, b):\n\treturn a + b\n");
    auto function = std::static_pointer_cast<FunctionDef>(program.statements[0]);
    REQUIRE(function->name == "add");
    REQUIRE(function->parameters.size() == 2);
    REQUIRE(function->parameters[0] == "a");
    REQUIRE(function->parameters[1] == "b");
    REQUIRE(function->body.size() == 1);
}

TEST_CASE("parser: lifecycle requires parentheses", "[parser]") {
    REQUIRE_THROWS_WITH(testutil::parse("fn on_kapcsol:\n\tpass\n"),
                        Catch::Contains("`(` is required after the function name"));
}

TEST_CASE("parser: START cannot have parameters", "[parser]") {
    REQUIRE_THROWS_WITH(testutil::parse("START(x):\n\tpass\n"),
                        Catch::Contains("START() cannot have parameters"));
}

TEST_CASE("parser: PROCESS parameter must be FPS", "[parser]") {
    REQUIRE_THROWS_WITH(testutil::parse("PROCESS(x):\n\tpass\n"),
                        Catch::Contains("the PROCESS parameter must be named `FPS`"));
}

TEST_CASE("parser: declaration needs a type", "[parser]") {
    REQUIRE_THROWS_WITH(testutil::parse("v a = 1\n"),
                        Catch::Contains("a type name is required in a declaration"));
}

TEST_CASE("parser: mat cannot match a literal", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v a int = 1\nmat 5:\n\t1:\n\t\tpass\n"),
        Catch::Contains("a literal cannot be used directly as the value checked by `mat`"));
}

TEST_CASE("parser: mat with only else is rejected", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v a int = 1\nmat a:\n\telse:\n\t\tpass\n"),
        Catch::Contains("EmptyMatchBody"));
}

TEST_CASE("parser: mat without a body is rejected", "[parser]") {
    REQUIRE_THROWS_AS(testutil::parse("v a int = 1\nmat a:\n"), ResirisSyntaxError);
}

TEST_CASE("parser: mat case needs a body", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v a int = 1\nmat a:\n\t1:\n"),
        Catch::Contains("MissingCaseBody"));
}

TEST_CASE("parser: mat duplicate case value", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v a int = 1\nmat a:\n\t1:\n\t\tpass\n\t1:\n\t\tpass\n"),
        Catch::Contains("SameCaseMultiCall"));
}

TEST_CASE("parser: nested mat is rejected", "[parser]") {
    std::string source =
        "v a int = 1\n"
        "mat a:\n"
        "\t1:\n"
        "\t\tmat a:\n"
        "\t\t\t1:\n"
        "\t\t\t\tpass\n";
    REQUIRE_THROWS_WITH(testutil::parse(source), Catch::Contains("NestedMatchError"));
}

TEST_CASE("parser: type() rejects a non type argument", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v a int = 1\nv b int = a.type(name)\n"),
        Catch::Contains("type() requires one of: int, float, string, bool"));
}

TEST_CASE("parser: FunctionalObject needs .new", "[parser]") {
    REQUIRE_THROWS_WITH(
        testutil::parse("v f FunctionalObject = g\n"),
        Catch::Contains("a FunctionalObject assignment must use `FunctionalObject.new(...)`"));
}

TEST_CASE("parser: errors carry line and column", "[parser]") {
    try {
        testutil::parse("v a int = 1\nv a = \n");
        FAIL("expected ResirisSyntaxError");
    } catch (const ResirisSyntaxError& error) {
        std::string message = error.what();
        REQUIRE(message.find("line 2") != std::string::npos);
        REQUIRE(message.find("column") != std::string::npos);
    }
}