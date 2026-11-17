#include "resiris/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_map>

#include "resiris/syntax_error.hpp"

namespace resiris {

namespace {

const std::unordered_map<std::string, TokenType>& keywords() {
    static const std::unordered_map<std::string, TokenType> map = {
        {"include", TokenType::INCLUDE},
        {"v", TokenType::V},
        {"c", TokenType::C},
        {"fn", TokenType::FN},
        {"START", TokenType::START},
        {"PROCESS", TokenType::PROCESS},
        {"if", TokenType::IF},
        {"elif", TokenType::ELIF},
        {"else", TokenType::ELSE},
        {"mat", TokenType::MAT},
        {"return", TokenType::RETURN},
        {"pass", TokenType::PASS},
        {"print_cmd", TokenType::PRINT_CMD},
        {"true", TokenType::TRUE},
        {"false", TokenType::FALSE},
        {"UnknownObject", TokenType::TYPE_UNKNOWN},
        {"int", TokenType::TYPE_INT},
        {"float", TokenType::TYPE_FLOAT},
        {"string", TokenType::TYPE_STRING},
        {"bool", TokenType::TYPE_BOOL},
        {"ModuleObject", TokenType::TYPE_MODULE_OBJECT},
        {"FunctionalObject", TokenType::TYPE_FUNCTIONAL_OBJECT},
    };
    return map;
}

const std::unordered_map<std::string, TokenType>& two_char_ops() {
    static const std::unordered_map<std::string, TokenType> map = {
        {"+=", TokenType::PLUS_ASSIGN},
        {"-=", TokenType::MINUS_ASSIGN},
        {"*=", TokenType::STAR_ASSIGN},
        {"/=", TokenType::SLASH_ASSIGN},
        {"==", TokenType::EQ},
        {"!=", TokenType::NE},
        {">=", TokenType::GE},
        {"<=", TokenType::LE},
    };
    return map;
}

const std::unordered_map<char, TokenType>& one_char_ops() {
    static const std::unordered_map<char, TokenType> map = {
        {'=', TokenType::ASSIGN},
        {'+', TokenType::PLUS},
        {'-', TokenType::MINUS},
        {'*', TokenType::STAR},
        {'/', TokenType::SLASH},
        {'%', TokenType::PERCENT},
        {'>', TokenType::GT},
        {'<', TokenType::LT},
        {':', TokenType::COLON},
        {',', TokenType::COMMA},
        {'(', TokenType::LPAREN},
        {')', TokenType::RPAREN},
        {'.', TokenType::DOT},
        {'[', TokenType::LBRACKET},
        {']', TokenType::RBRACKET},
    };
    return map;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

bool is_alnum(char c) {
    return is_digit(c) || is_alpha(c);
}

Token make_token(TokenType type, TokenValue value, int line, int column) {
    Token token;
    token.type = type;
    token.value = std::move(value);
    token.line = line;
    token.column = column;
    return token;
}

} // namespace

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT";
        case TokenType::EOF_: return "EOF";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::INCLUDE: return "INCLUDE";
        case TokenType::V: return "V";
        case TokenType::C: return "C";
        case TokenType::FN: return "FN";
        case TokenType::START: return "START";
        case TokenType::PROCESS: return "PROCESS";
        case TokenType::IF: return "IF";
        case TokenType::ELIF: return "ELIF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::MAT: return "MAT";
        case TokenType::RETURN: return "RETURN";
        case TokenType::PASS: return "PASS";
        case TokenType::PRINT_CMD: return "PRINT_CMD";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::TYPE_UNKNOWN: return "TYPE_UNKNOWN";
        case TokenType::TYPE_INT: return "TYPE_INT";
        case TokenType::TYPE_FLOAT: return "TYPE_FLOAT";
        case TokenType::TYPE_STRING: return "TYPE_STRING";
        case TokenType::TYPE_BOOL: return "TYPE_BOOL";
        case TokenType::TYPE_MODULE_OBJECT: return "TYPE_MODULE_OBJECT";
        case TokenType::TYPE_FUNCTIONAL_OBJECT: return "TYPE_FUNCTIONAL_OBJECT";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TokenType::STAR_ASSIGN: return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN: return "SLASH_ASSIGN";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::GT: return "GT";
        case TokenType::LT: return "LT";
        case TokenType::GE: return "GE";
        case TokenType::LE: return "LE";
        case TokenType::COLON: return "COLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::DOT: return "DOT";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
    }
    return "UNKNOWN";
}

std::vector<Token> Tokenizer::tokenize(const std::string& source) const {
    std::vector<Token> tokens;
    std::vector<int> indent_stack = {0};

    std::vector<std::string> lines;
    {
        std::string current;
        for (char c : source) {
            if (c == '\n') {
                lines.push_back(current);
                current.clear();
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            lines.push_back(current);
        }
    }

    std::size_t line_no = 0;
    for (const std::string& raw_line_obj : lines) {
        ++line_no;
        const std::string& raw_line = raw_line_obj;

        // Leading tabs/spaces.
        std::size_t leading = 0;
        while (leading < raw_line.size() &&
               (raw_line[leading] == ' ' || raw_line[leading] == '\t')) {
            ++leading;
        }
        std::size_t space_pos = raw_line.find(' ');
        if (space_pos != std::string::npos && space_pos < leading) {
            throw ResirisSyntaxError(
                "line " + std::to_string(line_no) +
                ": spaces cannot be used for indentation; use tabs");
        }

        std::size_t stripped_offset = 0;
        while (stripped_offset < raw_line.size() && raw_line[stripped_offset] == '\t') {
            ++stripped_offset;
        }
        const std::string stripped = raw_line.substr(stripped_offset);

        if (stripped.empty() ||
            (stripped.size() >= 2 && stripped.compare(0, 2, "##") == 0)) {
            continue;
        }

        const int indent = static_cast<int>(stripped_offset);

        if (indent > indent_stack.back()) {
            indent_stack.push_back(indent);
            tokens.push_back(make_token(TokenType::INDENT, indent, static_cast<int>(line_no), 1));
        } else if (indent < indent_stack.back()) {
            while (indent < indent_stack.back()) {
                indent_stack.pop_back();
                // The prototype reports the target indent of the line, not the
                // level that was just left.
                tokens.push_back(make_token(
                    TokenType::DEDENT, indent, static_cast<int>(line_no), 1));
            }
            if (indent != indent_stack.back()) {
                throw ResirisSyntaxError(
                    "line " + std::to_string(line_no) + ": invalid indentation");
            }
        }

        std::size_t i = stripped_offset;
        const std::size_t n = raw_line.size();

        while (i < n) {
            const char ch = raw_line[i];

            if (ch == ' ' || ch == '\t') {
                ++i;
                continue;
            }

            if (i + 1 < n && raw_line[i] == '#' && raw_line[i + 1] == '#') {
                break;
            }

            const int column = static_cast<int>(i + 1);

            // Two-character operators first.
            if (i + 1 < n) {
                const std::string two = raw_line.substr(i, 2);
                auto found = two_char_ops().find(two);
                if (found != two_char_ops().end()) {
                    tokens.push_back(make_token(
                        found->second, two, static_cast<int>(line_no), column));
                    i += 2;
                    continue;
                }
            }

            auto one = one_char_ops().find(ch);
            if (one != one_char_ops().end()) {
                tokens.push_back(make_token(
                    one->second, std::string(1, ch), static_cast<int>(line_no), column));
                ++i;
                continue;
            }

            if (ch == '"' || ch == '\'') {
                const char quote = ch;
                const std::size_t start = i;
                ++i;
                std::string value_chars;

                bool closed = false;
                while (i < n) {
                    if (raw_line[i] == '\\') {
                        if (i + 1 >= n) {
                            throw ResirisSyntaxError(
                                "line " + std::to_string(line_no) +
                                ", column " + std::to_string(i + 1) +
                                ": unterminated string");
                        }
                        const char escaped = raw_line[i + 1];
                        switch (escaped) {
                            case 'n': value_chars.push_back('\n'); break;
                            case 't': value_chars.push_back('\t'); break;
                            case '\\': value_chars.push_back('\\'); break;
                            case '"': value_chars.push_back('"'); break;
                            case '\'': value_chars.push_back('\''); break;
                            default: value_chars.push_back(escaped); break;
                        }
                        i += 2;
                        continue;
                    }

                    if (raw_line[i] == quote) {
                        ++i;
                        closed = true;
                        break;
                    }

                    value_chars.push_back(raw_line[i]);
                    ++i;
                }

                if (!closed) {
                    throw ResirisSyntaxError(
                        "line " + std::to_string(line_no) +
                        ", column " + std::to_string(start + 1) +
                        ": unterminated string");
                }

                tokens.push_back(make_token(
                    TokenType::STRING, value_chars, static_cast<int>(line_no),
                    static_cast<int>(start + 1)));
                continue;
            }

            if (is_digit(ch)) {
                const std::size_t start = i;
                while (i < n && is_digit(raw_line[i])) {
                    ++i;
                }

                TokenType token_type = TokenType::INTEGER;
                TokenValue value = static_cast<std::int64_t>(
                    std::stoll(raw_line.substr(start, i - start)));

                if (i < n && raw_line[i] == '.') {
                    if (i + 1 < n && is_digit(raw_line[i + 1])) {
                        ++i;
                        while (i < n && is_digit(raw_line[i])) {
                            ++i;
                        }
                        token_type = TokenType::FLOAT;
                        value = std::stod(raw_line.substr(start, i - start));
                    } else {
                        throw ResirisSyntaxError(
                            "line " + std::to_string(line_no) +
                            ", column " + std::to_string(i + 1) +
                            ": a digit is required after the decimal point");
                    }
                }

                tokens.push_back(make_token(
                    token_type, value, static_cast<int>(line_no),
                    static_cast<int>(start + 1)));
                continue;
            }

            if (is_alpha(ch) || ch == '_') {
                const std::size_t start = i;
                ++i;
                while (i < n && (is_alnum(raw_line[i]) || raw_line[i] == '_')) {
                    ++i;
                }

                const std::string word = raw_line.substr(start, i - start);
                auto keyword = keywords().find(word);
                TokenType token_type =
                    keyword != keywords().end() ? keyword->second : TokenType::IDENTIFIER;
                tokens.push_back(make_token(
                    token_type, word, static_cast<int>(line_no),
                    static_cast<int>(start + 1)));
                continue;
            }

            throw ResirisSyntaxError(
                "line " + std::to_string(line_no) +
                ", column " + std::to_string(column) +
                ": unknown character: '" + std::string(1, ch) + "'");
        }

        tokens.push_back(make_token(
            TokenType::NEWLINE, std::string("\\n"), static_cast<int>(line_no),
            static_cast<int>(n + 1)));
    }

    const int final_line = static_cast<int>(std::max<std::size_t>(1, lines.size()));
    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        tokens.push_back(make_token(TokenType::DEDENT, indent_stack.back(), final_line, 1));
    }

    tokens.push_back(make_token(TokenType::EOF_, std::monostate{}, final_line, 1));
    return tokens;
}

} // namespace resiris