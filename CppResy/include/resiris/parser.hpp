#pragma once

#include <cstddef>
#include <string>

#include "resiris/ast.hpp"
#include "resiris/token.hpp"

namespace resiris {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse();

private:
    // Owned: a Parser must stay valid even if the caller drops the vector.
    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    int mat_case_depth_ = 0;

    const Token& current() const;
    const Token& previous() const;
    bool at(TokenType type) const;
    const Token& advance();
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& token, const std::string& message) const;

    void skip_newlines();

    StmtPtr parse_statement();
    StmtPtr parse_statement_impl();
    StmtPtr parse_include();
    StmtPtr parse_declaration();
    StmtPtr parse_declaration_after_kind(const std::string& kind);
    ExprPtr parse_functional_object();
    StmtPtr parse_function();
    StmtPtr parse_named_function();
    std::vector<std::string> parse_parameter_list();
    StmtList parse_block();

    StmtPtr parse_mat();
    ExprPtr parse_mat_case_value();
    StmtList parse_mat_case_body();
    std::string mat_case_key(const Value& value) const;

    StmtPtr parse_if();
    StmtPtr parse_print_cmd();
    StmtPtr parse_return();
    StmtPtr parse_assignment_or_expression();

    ExprPtr parse_expression();
    ExprPtr parse_comparison();
    ExprPtr parse_term();
    ExprPtr parse_factor();
    ExprPtr parse_unary();
    ExprPtr parse_call();
    ExprPtr parse_primary();
};

std::string python_repr(const TokenValue& value);
std::string python_str(const Value& value);

} // namespace resiris