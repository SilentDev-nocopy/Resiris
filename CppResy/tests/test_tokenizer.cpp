#include "helpers.hpp"

using namespace resiris;

namespace {

// Collect the token types of a source, skipping nothing.
std::vector<TokenType> types(const std::string& source) {
    std::vector<TokenType> out;
    for (const auto& token : testutil::tokenize(source)) {
        out.push_back(token.type);
    }
    return out;
}

} // namespace

TEST_CASE("tokenizer: simple tokens", "[tokenizer]") {
    auto t = testutil::tokenize("v x int = 10\n");
    REQUIRE(t.size() >= 6);
    REQUIRE(t[0].type == TokenType::V);
    REQUIRE(t[1].type == TokenType::IDENTIFIER);
    REQUIRE(t[2].type == TokenType::TYPE_INT);
    REQUIRE(t[3].type == TokenType::ASSIGN);
    REQUIRE(t[4].type == TokenType::INTEGER);
    REQUIRE(std::get<std::int64_t>(t[4].value) == 10);
    REQUIRE(t[5].type == TokenType::NEWLINE);
}

TEST_CASE("tokenizer: string literals", "[tokenizer]") {
    auto t = testutil::tokenize("print_cmd(\"hello\\n\")\n");
    bool found_string = false;
    for (const auto& token : t) {
        if (token.type == TokenType::STRING) {
            found_string = true;
            REQUIRE(std::get<std::string>(token.value) == "hello\n");
        }
    }
    REQUIRE(found_string);
}

TEST_CASE("tokenizer: indent is tab based", "[tokenizer]") {
    REQUIRE_NOTHROW(testutil::tokenize("v a int = 1\n"));
    REQUIRE_THROWS_AS(testutil::tokenize(" v a int = 1\n"), ResirisSyntaxError);
}

TEST_CASE("tokenizer: tokens with indentation produce indent/dedent", "[tokenizer]") {
    auto t = types("\tprint_cmd(\"x\")\n");
    REQUIRE(t.front() == TokenType::INDENT);
    REQUIRE(t[t.size() - 2] == TokenType::DEDENT);
    REQUIRE(t.back() == TokenType::EOF_);
}

TEST_CASE("tokenizer: unterminated string is rejected", "[tokenizer]") {
    REQUIRE_THROWS_AS(testutil::tokenize("v s string = \"abc\n"), ResirisSyntaxError);
}

TEST_CASE("tokenizer: numbers", "[tokenizer]") {
    auto t = testutil::tokenize("v a float = 3.14\nv b int = 42\nv c int = 7\n");
    std::vector<double> values;
    for (const auto& token : t) {
        if (token.type == TokenType::INTEGER) {
            values.push_back(static_cast<double>(std::get<std::int64_t>(token.value)));
        } else if (token.type == TokenType::FLOAT) {
            values.push_back(std::get<double>(token.value));
        }
    }
    REQUIRE(values.size() == 3);
    REQUIRE(values[0] == 3.14);
    REQUIRE(values[1] == 42);
    REQUIRE(values[2] == 7);
}

TEST_CASE("tokenizer: comment is skipped", "[tokenizer]") {
    // The prototype uses `##` for comments, not `#`.
    std::string source = "## comment\nv a int = 1\n";
    auto t = testutil::tokenize(source);
    bool one_seen = false;
    for (const auto& token : t) {
        if (token.type == TokenType::INTEGER &&
            std::get<std::int64_t>(token.value) == 1) {
            one_seen = true;
        }
    }
    REQUIRE(one_seen);
    REQUIRE(t.front().type == TokenType::V);
}

TEST_CASE("tokenizer: keywords", "[tokenizer]") {
    auto t = testutil::tokenize("<include> RSMath\n");
    REQUIRE(t[0].type == TokenType::LT);
    REQUIRE(t[1].type == TokenType::INCLUDE);
    REQUIRE(t[2].type == TokenType::GT);
    REQUIRE(t[3].type == TokenType::IDENTIFIER);
    REQUIRE(std::get<std::string>(t[3].value) == "RSMath");
}