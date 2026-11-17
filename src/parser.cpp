#include "resiris/parser.hpp"

#include <cstdint>
#include <set>
#include <unordered_map>
#include <utility>

#include "resiris/syntax_error.hpp"

namespace resiris {

namespace {

bool is_type_token(TokenType type) {
    switch (type) {
        case TokenType::TYPE_UNKNOWN:
        case TokenType::TYPE_INT:
        case TokenType::TYPE_FLOAT:
        case TokenType::TYPE_STRING:
        case TokenType::TYPE_BOOL:
        case TokenType::TYPE_MODULE_OBJECT:
        case TokenType::TYPE_FUNCTIONAL_OBJECT:
            return true;
        default:
            return false;
    }
}

const std::unordered_map<TokenType, std::string>& type_tokens() {
    static const std::unordered_map<TokenType, std::string> map = {
        {TokenType::TYPE_UNKNOWN, "UnknownObject"},
        {TokenType::TYPE_INT, "int"},
        {TokenType::TYPE_FLOAT, "float"},
        {TokenType::TYPE_STRING, "string"},
        {TokenType::TYPE_BOOL, "bool"},
        {TokenType::TYPE_MODULE_OBJECT, "ModuleObject"},
        {TokenType::TYPE_FUNCTIONAL_OBJECT, "FunctionalObject"},
    };
    return map;
}

const std::unordered_map<TokenType, std::string>& assignment_tokens() {
    static const std::unordered_map<TokenType, std::string> map = {
        {TokenType::ASSIGN, "="},
        {TokenType::PLUS_ASSIGN, "+="},
        {TokenType::MINUS_ASSIGN, "-="},
        {TokenType::STAR_ASSIGN, "*="},
        {TokenType::SLASH_ASSIGN, "/="},
    };
    return map;
}

std::string type_name_of(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return "bool";
        case Value::Type::Int: return "int";
        case Value::Type::Float: return "float";
        case Value::Type::String: return "string";
        case Value::Type::ModuleObject: return "ModuleObject";
        case Value::Type::FunctionalObject: return "FunctionalObject";
        default: return "Undefined";
    }
}

} // namespace

std::string python_repr(const TokenValue& value) {
    if (const auto* str = std::get_if<std::string>(&value)) {
        std::string out = "'";
        for (char ch : *str) {
            switch (ch) {
                case '\\': out += "\\\\"; break;
                case '\'': out += "\\'"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(ch); break;
            }
        }
        out += "'";
        return out;
    }
    if (const auto* i = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*i);
    }
    if (const auto* d = std::get_if<double>(&value)) {
        return float_to_string(*d);
    }
    return "None";
}

std::string python_str(const Value& value) {
    switch (value.type) {
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: return value.string_value;
        case Value::Type::ModuleObject: return value.module_value ? value.module_value->to_string() : "ModuleObject";
        default: return "None";
    }
}

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::current() const {
    return tokens_[pos_];
}

const Token& Parser::previous() const {
    return tokens_[pos_ - 1];
}

bool Parser::at(TokenType type) const {
    return current().type == type;
}

const Token& Parser::advance() {
    const Token& token = current();
    if (token.type != TokenType::EOF_) {
        pos_ += 1;
    }
    return token;
}

bool Parser::match(TokenType type) {
    if (current().type == type) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    const Token& token = current();
    if (token.type != type) {
        error(token, message);
    }
    return advance();
}

void Parser::error(const Token& token, const std::string& message) const {
    throw ResirisSyntaxError(
        "line " + std::to_string(token.line) + ", column " +
        std::to_string(token.column) + ": " + message + "; got: " +
        std::string(token_type_name(token.type)) + " (" +
        python_repr(token.value) + ")");
}

void Parser::skip_newlines() {
    while (match(TokenType::NEWLINE)) {
    }
}

Program Parser::parse() {
    Program program;
    skip_newlines();
    while (!at(TokenType::EOF_)) {
        program.statements.push_back(parse_statement());
        skip_newlines();
    }
    return program;
}

StmtPtr Parser::parse_statement() {
    const Token& token = current();
    StmtPtr statement = parse_statement_impl();
    statement->source_line = token.line;
    statement->source_column = token.column;
    return statement;
}

StmtPtr Parser::parse_statement_impl() {
    const Token& token = current();

    if (token.type == TokenType::LT && pos_ + 1 < tokens_.size() &&
        tokens_[pos_ + 1].type == TokenType::INCLUDE) {
        return parse_include();
    }
    if (token.type == TokenType::V) {
        return parse_declaration();
    }
    if (token.type == TokenType::C) {
        return parse_declaration();
    }
    if (token.type == TokenType::FN) {
        return parse_function();
    }
    if (token.type == TokenType::START || token.type == TokenType::PROCESS) {
        return parse_named_function();
    }
    if (token.type == TokenType::IF) {
        return parse_if();
    }
    if (token.type == TokenType::MAT) {
        if (mat_case_depth_ > 0) {
            error(token,
                  "Cannot call match inside a match function. Error code:\"NestedMatchError\"");
        }
        return parse_mat();
    }
    if (token.type == TokenType::RETURN) {
        return parse_return();
    }
    if (token.type == TokenType::PASS) {
        advance();
        while (!at(TokenType::NEWLINE) && !at(TokenType::EOF_)) {
            advance();
        }
        match(TokenType::NEWLINE);
        return std::make_shared<PassStmt>();
    }
    if (token.type == TokenType::PRINT_CMD) {
        return parse_print_cmd();
    }

    return parse_assignment_or_expression();
}

StmtPtr Parser::parse_include() {
    expect(TokenType::LT, "`<` is required before `include`");
    expect(TokenType::INCLUDE, "`include` is required inside `<...>`");
    expect(TokenType::GT, "`>` is required after `include`");

    std::vector<std::string> modules;
    while (true) {
        const Token& module =
            expect(TokenType::IDENTIFIER, "a module name is required after `<include>`");
        modules.push_back(std::get<std::string>(module.value));
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::NEWLINE, "a line ending is required after `<include>`");
    return std::make_shared<Include>(std::move(modules));
}

StmtPtr Parser::parse_declaration() {
    std::string kind = std::get<std::string>(advance().value);
    return parse_declaration_after_kind(kind);
}

StmtPtr Parser::parse_declaration_after_kind(const std::string& kind) {
    const Token& name =
        expect(TokenType::IDENTIFIER, "the first name in a declaration must be an identifier");

    const Token& type_token = current();
    if (!is_type_token(type_token.type)) {
        error(type_token, "a type name is required in a declaration");
    }
    std::string type_name = type_tokens().at(type_token.type);
    advance();

    ExprPtr value;
    if (match(TokenType::ASSIGN)) {
        if (type_name == "FunctionalObject") {
            value = parse_functional_object();
        } else {
            value = parse_expression();
        }
    }

    bool functional_object_value =
        type_name == "FunctionalObject" && value &&
        value->kind == ExprKind::FunctionalObjectDef;
    if (!functional_object_value) {
        expect(TokenType::NEWLINE, "a line ending is required at the end of a declaration");
    }
    return std::make_shared<Declaration>(
        kind, std::get<std::string>(name.value), type_name, std::move(value));
}

ExprPtr Parser::parse_functional_object() {
    const Token& name = current();
    if (name.type != TokenType::TYPE_FUNCTIONAL_OBJECT) {
        error(name, "a FunctionalObject assignment must use `FunctionalObject.new(...)`");
    }
    advance();
    expect(TokenType::DOT, "a dot is required after `FunctionalObject`");
    const Token& new_token =
        expect(TokenType::IDENTIFIER, "`new` is required after `FunctionalObject.`");
    if (std::get<std::string>(new_token.value) != "new") {
        error(new_token, "the FunctionalObject constructor must be named `new`");
    }
    std::vector<std::string> parameters = parse_parameter_list();
    expect(TokenType::COLON, "the FunctionalObject header must end with `:`");
    expect(TokenType::NEWLINE, "a line ending is required after `:`");
    StmtList body = parse_block();
    return std::make_shared<FunctionalObjectDef>(std::move(parameters), std::move(body));
}

StmtPtr Parser::parse_function() {
    advance();
    const Token& name =
        expect(TokenType::IDENTIFIER, "a function name is required after `fn`");
    std::vector<std::string> parameters = parse_parameter_list();
    expect(TokenType::COLON, "the function header must end with `:`");
    expect(TokenType::NEWLINE, "a line ending is required after `:`");
    StmtList body = parse_block();
    return std::make_shared<FunctionDef>(
        std::get<std::string>(name.value), std::move(parameters), std::move(body));
}

StmtPtr Parser::parse_named_function() {
    const Token& name_token = advance();

    expect(TokenType::LPAREN, "`(` is required after the lifecycle name");

    std::optional<std::string> parameter_name;
    if (name_token.type == TokenType::START) {
        if (!at(TokenType::RPAREN)) {
            error(current(), "START() cannot have parameters");
        }
        advance();
    } else {  // PROCESS
        const Token& parameter =
            expect(TokenType::IDENTIFIER, "`FPS` is required as the PROCESS parameter");
        if (std::get<std::string>(parameter.value) != "FPS") {
            error(parameter, "the PROCESS parameter must be named `FPS`");
        }
        parameter_name = std::get<std::string>(parameter.value);
        expect(TokenType::RPAREN, "missing `)` in the PROCESS parameter list");
    }

    expect(TokenType::COLON, "the lifecycle header must end with `:`");
    expect(TokenType::NEWLINE, "a line ending is required after `:`");
    StmtList body = parse_block();
    return std::make_shared<LifecycleDef>(
        std::get<std::string>(name_token.value), std::move(parameter_name), std::move(body));
}

std::vector<std::string> Parser::parse_parameter_list() {
    expect(TokenType::LPAREN, "`(` is required after the function name");
    std::vector<std::string> parameters;

    if (!at(TokenType::RPAREN)) {
        while (true) {
            const Token& param =
                expect(TokenType::IDENTIFIER, "a function parameter must be an identifier");
            parameters.push_back(std::get<std::string>(param.value));
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
    }

    expect(TokenType::RPAREN, "missing `)` in the parameter list");
    return parameters;
}

StmtList Parser::parse_block() {
    expect(TokenType::INDENT, "an indented line is required for the block");
    skip_newlines();

    StmtList statements;
    while (!at(TokenType::DEDENT) && !at(TokenType::EOF_)) {
        statements.push_back(parse_statement());
        skip_newlines();
    }

    expect(TokenType::DEDENT, "missing block terminator");
    return statements;
}

StmtPtr Parser::parse_mat() {
    advance();  // mat

    ExprPtr value = parse_expression();
    if (value->kind == ExprKind::Literal) {
        error(previous(), "a literal cannot be used directly as the value checked by `mat`");
    }
    expect(TokenType::COLON, "`mat` must end with `:`");
    expect(TokenType::NEWLINE, "a line ending is required after `mat`");

    expect(TokenType::INDENT, "an indented line is required for the mat body");
    skip_newlines();

    std::vector<MatCase> cases;
    std::optional<StmtList> else_body;
    std::set<std::string> case_keys;
    std::string case_type;
    bool type_match_mode =
        value->kind == ExprKind::TypeConversion &&
        std::static_pointer_cast<TypeConversionExpr>(value)->target_type == std::nullopt;

    while (!at(TokenType::DEDENT) && !at(TokenType::EOF_)) {
        if (at(TokenType::ELSE)) {
            if (else_body) {
                error(current(), "multiple `else` branches are not allowed in `mat`");
            }
            advance();
            expect(TokenType::COLON, "`else` must end with `:`");
            expect(TokenType::NEWLINE, "a line ending is required after `else`");
            else_body = parse_mat_case_body();
            skip_newlines();
            if (!at(TokenType::DEDENT)) {
                error(current(), "`else` must be the last branch of `mat`");
            }
            continue;
        }

        bool type_case =
            is_type_token(current().type) && current().type != TokenType::TYPE_UNKNOWN;

        Value case_value;
        std::string key;
        std::string current_case_type;
        bool used_type_case = false;
        if (type_case) {
            if (!type_match_mode) {
                error(current(), "a type case can only be used when `mat` checks `.type()`");
            }
            const Token& token = advance();
            case_value = Value::make_string(type_tokens().at(token.type));
            key = std::string("type:") + case_value.string_value;
            current_case_type = "type";
            used_type_case = true;
        } else {
            ExprPtr case_expr = parse_mat_case_value();
            case_value = std::static_pointer_cast<Literal>(case_expr)->value;
            key = std::string("value:") + type_name_of(case_value) + ":" + mat_case_key(case_value);
            current_case_type = std::string("value:") + type_name_of(case_value);
        }

        if (case_type.empty()) {
            case_type = current_case_type;
        } else if (case_type != current_case_type) {
            error(current(), "different case types cannot be mixed in the same `mat`");
        }

        if (case_keys.count(key) != 0) {
            error(previous(),
                  python_str(case_value) +
                      " is already a used case value! Error code:\"SameCaseMultiCall\"");
        }
        case_keys.insert(key);

        expect(TokenType::COLON, "a mat case must end with `:`");
        expect(TokenType::NEWLINE, "a line ending is required after a mat case");
        StmtList body = parse_mat_case_body();

        cases.emplace_back(case_value, std::move(body), used_type_case);
        skip_newlines();
    }

    if (cases.empty()) {
        error(current(), "mat statement has an empty body. Error code:\"EmptyMatchBody\"");
    }

    expect(TokenType::DEDENT, "missing mat block terminator");
    return std::make_shared<MatStmt>(
        std::move(value), std::move(cases), else_body.value_or(StmtList{}));
}

ExprPtr Parser::parse_mat_case_value() {
    const Token& token = current();
    if (token.type == TokenType::INTEGER) {
        advance();
        return std::make_shared<Literal>(Value::make_int(std::get<std::int64_t>(token.value)));
    }
    if (token.type == TokenType::FLOAT) {
        advance();
        return std::make_shared<Literal>(Value::make_float(std::get<double>(token.value)));
    }
    if (token.type == TokenType::STRING) {
        advance();
        return std::make_shared<Literal>(Value::make_string(std::get<std::string>(token.value)));
    }
    if (token.type == TokenType::TRUE) {
        advance();
        return std::make_shared<Literal>(Value::make_bool(true));
    }
    if (token.type == TokenType::FALSE) {
        advance();
        return std::make_shared<Literal>(Value::make_bool(false));
    }
    error(token,
          python_repr(token.value) + " is an invalid case value. Error code:\"InvalidCaseValue\"");
}

StmtList Parser::parse_mat_case_body() {
    if (!at(TokenType::INDENT)) {
        error(current(), "case has no body. Error code:\"MissingCaseBody\"");
    }
    mat_case_depth_ += 1;
    StmtList body = parse_block();
    mat_case_depth_ -= 1;
    return body;
}

std::string Parser::mat_case_key(const Value& value) const {
    switch (value.type) {
        case Value::Type::Int: return std::to_string(value.int_value);
        case Value::Type::Float: return float_to_string(value.float_value);
        case Value::Type::String: return value.string_value;
        case Value::Type::Bool: return value.bool_value ? "True" : "False";
        default: return std::string();
    }
}

StmtPtr Parser::parse_if() {
    advance();  // if
    ExprPtr condition = parse_expression();
    expect(TokenType::COLON, "`if` must end with `:`");
    expect(TokenType::NEWLINE, "a line ending is required after `if`");
    StmtList body = parse_block();

    std::vector<std::pair<ExprPtr, StmtList>> elif_blocks;
    while (match(TokenType::ELIF)) {
        ExprPtr elif_condition = parse_expression();
        expect(TokenType::COLON, "`elif` must end with `:`");
        expect(TokenType::NEWLINE, "a line ending is required after `elif`");
        StmtList elif_body = parse_block();
        elif_blocks.emplace_back(std::move(elif_condition), std::move(elif_body));
    }

    StmtList else_body;
    if (match(TokenType::ELSE)) {
        expect(TokenType::COLON, "`else` must end with `:`");
        expect(TokenType::NEWLINE, "a line ending is required after `else`");
        else_body = parse_block();
    }

    return std::make_shared<IfStmt>(std::move(condition), std::move(body),
                                    std::move(elif_blocks), std::move(else_body));
}

StmtPtr Parser::parse_print_cmd() {
    advance();  // print_cmd
    expect(TokenType::LPAREN, "`(` is required after `print_cmd`");
    ExprPtr expression = parse_expression();
    expect(TokenType::RPAREN, "missing `)` in the `print_cmd` call");
    expect(TokenType::NEWLINE, "a line ending is required after `print_cmd`");
    return std::make_shared<PrintCmdStmt>(std::move(expression));
}

StmtPtr Parser::parse_return() {
    advance();  // return
    if (at(TokenType::NEWLINE)) {
        advance();
        return std::make_shared<ReturnStmt>(ExprPtr{nullptr});
    }

    ExprPtr value = parse_expression();
    expect(TokenType::NEWLINE, "a line ending is required after `return`");
    return std::make_shared<ReturnStmt>(std::move(value));
}

StmtPtr Parser::parse_assignment_or_expression() {
    ExprPtr expression = parse_expression();

    if (expression->kind == ExprKind::Name &&
        assignment_tokens().count(current().type) != 0) {
        const Token& op_token = advance();
        std::string op = assignment_tokens().at(op_token.type);
        ExprPtr value = parse_expression();
        expect(TokenType::NEWLINE, "a line ending is required after the assignment");
        return std::make_shared<Assignment>(
            std::static_pointer_cast<Name>(expression)->name, std::move(op), std::move(value));
    }

    expect(TokenType::NEWLINE, "a line ending is required after the expression");
    return std::make_shared<ExpressionStmt>(std::move(expression));
}

ExprPtr Parser::parse_expression() {
    return parse_comparison();
}

ExprPtr Parser::parse_comparison() {
    ExprPtr expr = parse_term();

    while (current().type == TokenType::EQ || current().type == TokenType::NE ||
           current().type == TokenType::GT || current().type == TokenType::LT ||
           current().type == TokenType::GE || current().type == TokenType::LE) {
        std::string op = std::get<std::string>(advance().value);
        ExprPtr right = parse_term();
        expr = std::make_shared<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_term() {
    ExprPtr expr = parse_factor();

    while (current().type == TokenType::PLUS || current().type == TokenType::MINUS) {
        std::string op = std::get<std::string>(advance().value);
        ExprPtr right = parse_factor();
        expr = std::make_shared<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_factor() {
    ExprPtr expr = parse_unary();

    while (current().type == TokenType::STAR || current().type == TokenType::SLASH ||
           current().type == TokenType::PERCENT) {
        std::string op = std::get<std::string>(advance().value);
        ExprPtr right = parse_unary();
        expr = std::make_shared<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_unary() {
    if (match(TokenType::PLUS)) {
        return std::make_shared<UnaryExpr>("+", parse_unary());
    }
    if (match(TokenType::MINUS)) {
        return std::make_shared<UnaryExpr>("-", parse_unary());
    }
    return parse_call();
}

ExprPtr Parser::parse_call() {
    ExprPtr expr = parse_primary();

    while (true) {
        if (match(TokenType::LPAREN)) {
            std::vector<ExprPtr> arguments;
            if (!at(TokenType::RPAREN)) {
                while (true) {
                    arguments.push_back(parse_expression());
                    if (!match(TokenType::COMMA)) {
                        break;
                    }
                }
            }
            expect(TokenType::RPAREN, "missing `)` in the call");
            expr = std::make_shared<CallExpr>(std::move(expr), std::move(arguments));
            continue;
        }

        if (match(TokenType::LBRACKET)) {
            if (expr->kind != ExprKind::Name) {
                error(current(), "module constant access requires a module name before `[`");
            }

            if (current().type != TokenType::IDENTIFIER) {
                error(current(), "a constant name is required inside `[]`");
            }

            const Token& constant = advance();
            expect(TokenType::RBRACKET, "missing `]` in module constant access");
            expr = std::make_shared<ModuleConstantAccessExpr>(
                std::static_pointer_cast<Name>(expr)->name,
                std::get<std::string>(constant.value));
            continue;
        }

        if (match(TokenType::DOT)) {
            std::string method;
            if (current().type == TokenType::IDENTIFIER || current().type == TokenType::TYPE_STRING) {
                method = std::get<std::string>(advance().value);
            } else {
                error(current(), "a member name is required after `.`");
            }

            if (method != "type" && method != "string") {
                if (expr->kind == ExprKind::Name) {
                    expr = std::make_shared<ModuleAccessExpr>(
                        std::static_pointer_cast<Name>(expr)->name, method);
                } else {
                    expr = std::make_shared<ObjectAccessExpr>(std::move(expr), method);
                }
                continue;
            }

            expect(TokenType::LPAREN, "`(` is required after `." + method + "`");

            if (method == "string") {
                if (!match(TokenType::RPAREN)) {
                    error(current(), "string() receives no arguments");
                }
                expr = std::make_shared<TypeConversionExpr>(std::move(expr), std::string("string"));
                continue;
            }

            static const std::unordered_map<TokenType, std::string> target_types = {
                {TokenType::TYPE_INT, "int"},
                {TokenType::TYPE_FLOAT, "float"},
                {TokenType::TYPE_STRING, "string"},
                {TokenType::TYPE_BOOL, "bool"},
            };

            // type() with no argument queries the caller's current type.
            if (match(TokenType::RPAREN)) {
                expr = std::make_shared<TypeConversionExpr>(std::move(expr), std::nullopt);
                continue;
            }

            const Token& target = current();
            auto target_type = target_types.find(target.type);
            if (target_type == target_types.end()) {
                error(target, "type() requires one of: int, float, string, bool");
            }

            advance();
            std::string target_type_name = target_type->second;

            if (!at(TokenType::RPAREN)) {
                error(current(), "type() receives only 1 argument");
            }

            advance();
            expr = std::make_shared<TypeConversionExpr>(
                std::move(expr), std::optional<std::string>(target_type_name));
            continue;
        }

        return expr;
    }
}

ExprPtr Parser::parse_primary() {
    const Token& token = current();

    if (token.type == TokenType::LPAREN) {
        advance();
        ExprPtr expression = parse_expression();
        expect(TokenType::RPAREN, "missing `)` in the parenthesized expression");
        return expression;
    }

    if (token.type == TokenType::INTEGER) {
        advance();
        return std::make_shared<Literal>(Value::make_int(std::get<std::int64_t>(token.value)));
    }

    if (token.type == TokenType::TRUE) {
        advance();
        return std::make_shared<Literal>(Value::make_bool(true));
    }

    if (token.type == TokenType::FALSE) {
        advance();
        return std::make_shared<Literal>(Value::make_bool(false));
    }

    if (token.type == TokenType::FLOAT) {
        advance();
        return std::make_shared<Literal>(Value::make_float(std::get<double>(token.value)));
    }

    if (token.type == TokenType::STRING) {
        advance();
        return std::make_shared<Literal>(Value::make_string(std::get<std::string>(token.value)));
    }

    if (token.type == TokenType::IDENTIFIER) {
        advance();
        return std::make_shared<Name>(std::get<std::string>(token.value));
    }

    if (token.type == TokenType::START) {
        advance();
        return std::make_shared<Name>("start");
    }

    if (token.type == TokenType::PROCESS) {
        advance();
        return std::make_shared<Name>("process");
    }

    if (is_type_token(token.type)) {
        error(token, "a type name is not an expression here");
    }

    error(token, "a valid expression was expected");
}

} // namespace resiris