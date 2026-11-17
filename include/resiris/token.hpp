#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace resiris {

enum class TokenType : std::uint8_t {
    // Structure
    NEWLINE,
    INDENT,
    DEDENT,
    EOF_,

    // Names / literals
    IDENTIFIER,
    INTEGER,
    FLOAT,
    STRING,

    // Keywords
    INCLUDE,
    V,
    C,
    FN,
    START,
    PROCESS,
    IF,
    ELIF,
    ELSE,
    MAT,
    RETURN,
    PASS,
    PRINT_CMD,
    TRUE,
    FALSE,

    // Types
    TYPE_UNKNOWN,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_MODULE_OBJECT,
    TYPE_FUNCTIONAL_OBJECT,

    // Operators
    ASSIGN,
    PLUS_ASSIGN,
    MINUS_ASSIGN,
    STAR_ASSIGN,
    SLASH_ASSIGN,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,

    EQ,
    NE,
    GT,
    LT,
    GE,
    LE,

    // Punctuation
    COLON,
    COMMA,
    LPAREN,
    RPAREN,
    DOT,
    LBRACKET,
    RBRACKET,
};

const char* token_type_name(TokenType type);

using TokenValue = std::variant<std::monostate, std::string, std::int64_t, double>;

struct Token {
    TokenType type = TokenType::EOF_;
    TokenValue value;
    int line = 0;
    int column = 0;

    bool is(TokenType t) const { return type == t; }
};

} // namespace resiris