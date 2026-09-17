from __future__ import annotations

from dataclasses import asdict, is_dataclass
from typing import Optional

from .tokenizer import Token, TokenType, ResirisSyntaxError
from .ast_nodes import (
    Program,
    Include,
    Declaration,
    FunctionDef,
    IfStmt,
    ReturnStmt,
    PassStmt,
    PrintCmdStmt,
    Assignment,
    ExpressionStmt,
    Literal,
    Name,
    UnaryExpr,
    BinaryExpr,
    CallExpr,
    FunctionalObjectDef,
    TypeConversionExpr,
    MatStmt,
    MatCase,
    ModuleAccessExpr,
    ModuleConstantAccessExpr,
    LifecycleDef,
)


class Parser:
    """
    Resiris parser for the currently defined core grammar.

    It builds an AST. It does NOT execute code.
    """

    TYPE_TOKENS = {
        TokenType.TYPE_UNKNOWN: "UnknownObject",
        TokenType.TYPE_INT: "int",
        TokenType.TYPE_FLOAT: "float",
        TokenType.TYPE_STRING: "string",
        TokenType.TYPE_BOOL: "bool",
        TokenType.TYPE_MODULE_OBJECT: "ModuleObject",
        TokenType.TYPE_FUNCTIONAL_OBJECT: "FunctionalObject",
    }

    ASSIGNMENT_TOKENS = {
        TokenType.ASSIGN: "=",
        TokenType.PLUS_ASSIGN: "+=",
        TokenType.MINUS_ASSIGN: "-=",
        TokenType.STAR_ASSIGN: "*=",
        TokenType.SLASH_ASSIGN: "/=",
    }

    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0
        self.mat_case_depth = 0

    def current(self) -> Token:
        return self.tokens[self.pos]

    def previous(self) -> Token:
        return self.tokens[self.pos - 1]

    def at(self, token_type: TokenType) -> bool:
        return self.current().type is token_type

    def advance(self) -> Token:
        token = self.current()
        if token.type is not TokenType.EOF:
            self.pos += 1
        return token

    def match(self, *types: TokenType) -> Optional[Token]:
        if self.current().type in types:
            return self.advance()
        return None

    def expect(self, token_type: TokenType, message: str) -> Token:
        token = self.current()
        if token.type is not token_type:
            self.error(token, message)
        return self.advance()

    def error(self, token: Token, message: str) -> None:
        raise ResirisSyntaxError(
            f"line {token.line}, column {token.column}: {message}; got: "
            f"{token.type.name} ({token.value!r})"
        )

    def parse(self) -> Program:
        statements = []
        self.skip_newlines()

        while not self.at(TokenType.EOF):
            statements.append(self.parse_statement())
            self.skip_newlines()

        return Program(statements)

    def skip_newlines(self) -> None:
        while self.match(TokenType.NEWLINE):
            pass

    def parse_statement(self):
        """Parse one statement and attach its source position to the AST node."""
        token = self.current()
        statement = self._parse_statement_impl()
        statement.source_line = token.line
        statement.source_column = token.column
        return statement

    def _parse_statement_impl(self):
        token = self.current()

        if token.type is TokenType.LT and self.pos + 1 < len(self.tokens) and self.tokens[self.pos + 1].type is TokenType.INCLUDE:
            return self.parse_include()
        if token.type is TokenType.V:
            return self.parse_declaration()
        if token.type is TokenType.C:
            return self.parse_declaration()
        if token.type is TokenType.FN:
            return self.parse_function()
        if token.type in (TokenType.START, TokenType.PROCESS):
            return self.parse_named_function()
        if token.type is TokenType.IF:
            return self.parse_if()
        if token.type is TokenType.MAT:
            if self.mat_case_depth > 0:
                self.error(
                    token,
                    'Cannot call match inside a match function. Error code:"NestedMatchError"'
                )
            return self.parse_mat()
        if token.type is TokenType.RETURN:
            return self.parse_return()
        if token.type is TokenType.PASS:
            # `pass` at the start of a line skips the entire current line.
            # The tokenizer marks every line ending with a NEWLINE token,
            # so the parser consumes the complete line after `pass`.
            self.advance()
            while not self.at(TokenType.NEWLINE) and not self.at(TokenType.EOF):
                self.advance()
            self.match(TokenType.NEWLINE)
            return PassStmt()

        if token.type is TokenType.PRINT_CMD:
            return self.parse_print_cmd()

        return self.parse_assignment_or_expression()

    def parse_include(self) -> Include:
        self.expect(TokenType.LT, "`<` is required before `include`")
        self.expect(TokenType.INCLUDE, "`include` is required inside `<...>`")
        self.expect(TokenType.GT, "`>` is required after `include`")

        modules: list[str] = []
        while True:
            module = self.expect(
                TokenType.IDENTIFIER,
                "a module name is required after `<include>`"
            )
            modules.append(module.value)

            if not self.match(TokenType.COMMA):
                break

        self.expect(TokenType.NEWLINE, "a line ending is required after `<include>`")
        return Include(modules)

    def parse_declaration(self) -> Declaration:
        kind = self.advance().value
        return self.parse_declaration_after_kind(kind)

    def parse_declaration_after_kind(self, kind: str) -> Declaration:
        name = self.expect(
            TokenType.IDENTIFIER,
            "the first name in a declaration must be an identifier"
        )

        type_token = self.current()
        if type_token.type not in self.TYPE_TOKENS:
            self.error(
                type_token,
                "a type name is required in a declaration"
            )
        type_name = self.TYPE_TOKENS[self.advance().type]

        value = None
        if self.match(TokenType.ASSIGN):
            if type_name == "FunctionalObject":
                value = self.parse_functional_object()
            else:
                value = self.parse_expression()

        if not (type_name == "FunctionalObject" and isinstance(value, FunctionalObjectDef)):
            self.expect(TokenType.NEWLINE, "a line ending is required at the end of a declaration")
        return Declaration(kind, name.value, type_name, value)

    def parse_functional_object(self) -> FunctionalObjectDef:
        name = self.current()
        if name.type is not TokenType.TYPE_FUNCTIONAL_OBJECT:
            self.error(name, "a FunctionalObject assignment must use `FunctionalObject.new(...)`")
        self.advance()
        self.expect(TokenType.DOT, "a dot is required after `FunctionalObject`")
        new_token = self.expect(TokenType.IDENTIFIER, "`new` is required after `FunctionalObject.`")
        if new_token.value != "new":
            self.error(new_token, "the FunctionalObject constructor must be named `new`")
        parameters = self.parse_parameter_list()
        self.expect(TokenType.COLON, "the FunctionalObject header must end with `:`")
        self.expect(TokenType.NEWLINE, "a line ending is required after `:`")
        body = self.parse_block()
        return FunctionalObjectDef(parameters, body)

    def parse_function(self) -> FunctionDef:
        self.advance()  # fn
        name = self.expect(TokenType.IDENTIFIER, "a function name is required after `fn`")
        parameters = self.parse_parameter_list()
        self.expect(TokenType.COLON, "the function header must end with `:`")
        self.expect(TokenType.NEWLINE, "a line ending is required after `:`")
        body = self.parse_block()
        return FunctionDef(name.value, parameters, body)

    def parse_named_function(self) -> LifecycleDef:
        name_token = self.advance()

        self.expect(TokenType.LPAREN, "`(` is required after the lifecycle name")

        if name_token.type is TokenType.START:
            if not self.at(TokenType.RPAREN):
                self.error(
                    self.current(),
                    "START() cannot have parameters",
                )
            self.advance()
            parameter_name = None

        else:  # PROCESS
            parameter = self.expect(
                TokenType.IDENTIFIER,
                "`FPS` is required as the PROCESS parameter",
            )
            if parameter.value != "FPS":
                self.error(
                    parameter,
                    "the PROCESS parameter must be named `FPS`",
                )
            parameter_name = parameter.value
            self.expect(TokenType.RPAREN, "missing `)` in the PROCESS parameter list")

        self.expect(TokenType.COLON, "the lifecycle header must end with `:`")
        self.expect(TokenType.NEWLINE, "a line ending is required after `:`")
        body = self.parse_block()
        return LifecycleDef(name_token.value, parameter_name, body)

    def parse_parameter_list(self) -> list[str]:
        self.expect(TokenType.LPAREN, "`(` is required after the function name")
        parameters: list[str] = []

        if not self.at(TokenType.RPAREN):
            while True:
                param = self.expect(
                    TokenType.IDENTIFIER,
                    "a function parameter must be an identifier"
                )
                parameters.append(param.value)
                if not self.match(TokenType.COMMA):
                    break

        self.expect(TokenType.RPAREN, "missing `)` in the parameter list")
        return parameters

    def parse_block(self) -> list[object]:
        self.expect(TokenType.INDENT, "an indented line is required for the block")
        self.skip_newlines()

        statements = []
        while not self.at(TokenType.DEDENT) and not self.at(TokenType.EOF):
            statements.append(self.parse_statement())
            self.skip_newlines()

        self.expect(TokenType.DEDENT, "missing block terminator")
        return statements

    def parse_mat(self) -> MatStmt:
        self.advance()  # mat

        value = self.parse_expression()
        if isinstance(value, Literal):
            self.error(
                self.previous(),
                "a literal cannot be used directly as the value checked by `mat`"
            )
        self.expect(TokenType.COLON, "`mat` must end with `:`")
        self.expect(TokenType.NEWLINE, "a line ending is required after `mat`")

        self.expect(TokenType.INDENT, "an indented line is required for the mat body")
        self.skip_newlines()

        cases: list[MatCase] = []
        else_body = None
        case_keys = set()
        case_type = None
        type_match_mode = isinstance(value, TypeConversionExpr) and value.target_type is None

        while not self.at(TokenType.DEDENT) and not self.at(TokenType.EOF):
            # A nested `mat` directly inside a case is forbidden by the
            # Resiris mat specification. Nested mat inside other valid blocks
            # is handled normally by their respective parser.
            if self.at(TokenType.ELSE):
                if else_body is not None:
                    self.error(self.current(), "multiple `else` branches are not allowed in `mat`")
                self.advance()
                self.expect(TokenType.COLON, "`else` must end with `:`")
                self.expect(TokenType.NEWLINE, "a line ending is required after `else`")
                else_body = self.parse_mat_case_body()
                self.skip_newlines()
                if not self.at(TokenType.DEDENT):
                    self.error(self.current(), "`else` must be the last branch of `mat`")
                continue

            type_case = self.current().type in self.TYPE_TOKENS and self.current().type != TokenType.TYPE_UNKNOWN
            if type_case:
                if not type_match_mode:
                    self.error(
                        self.current(),
                        "a type case can only be used when `mat` checks `.type()`"
                    )
                token = self.advance()
                case_value = self.TYPE_TOKENS[token.type]
                key = ("type", case_value)
                current_case_type = ("type",)
            else:
                case_expr = self.parse_mat_case_value()
                case_value = case_expr.value
                key = ("value", self._mat_case_key(case_value))
                current_case_type = ("value", type(case_value).__name__)

            if case_type is None:
                case_type = current_case_type
            elif case_type != current_case_type:
                self.error(
                    self.current(),
                    "different case types cannot be mixed in the same `mat`"
                )

            if key in case_keys:
                self.error(self.previous(), f'{case_value} is already a used case value! Error code:"SameCaseMultiCall"')
            case_keys.add(key)

            self.expect(TokenType.COLON, "a mat case must end with `:`")
            self.expect(TokenType.NEWLINE, "a line ending is required after a mat case")
            body = self.parse_mat_case_body()

            cases.append(MatCase(case_value, body, type_case=type_case))
            self.skip_newlines()

        if not cases and else_body is not None:
            self.error(self.current(), 'mat statement has an empty body. Error code:"EmptyMatchBody"')
        if not cases:
            self.error(self.current(), 'mat statement has an empty body. Error code:"EmptyMatchBody"')

        self.expect(TokenType.DEDENT, "missing mat block terminator")
        return MatStmt(value, cases, else_body)

    def parse_mat_case_value(self):
        token = self.current()
        if token.type is TokenType.INTEGER:
            self.advance()
            return Literal(token.value)
        if token.type is TokenType.FLOAT:
            self.advance()
            return Literal(token.value)
        if token.type is TokenType.STRING:
            self.advance()
            return Literal(token.value)
        if token.type is TokenType.TRUE:
            self.advance()
            return Literal(True)
        if token.type is TokenType.FALSE:
            self.advance()
            return Literal(False)
        self.error(token, f'{token.value!r} is an invalid case value. Error code:"InvalidCaseValue"')

    def parse_mat_case_body(self):
        if not self.at(TokenType.INDENT):
            self.error(self.current(), 'case has no body. Error code:"MissingCaseBody"')
        self.mat_case_depth += 1
        try:
            return self.parse_block()
        finally:
            self.mat_case_depth -= 1

    def _mat_case_key(self, value):
        # bool is a subclass of int in Python; include the exact runtime type
        # so true and 1 remain distinct case values.
        return (type(value).__name__, value)

    def parse_if(self) -> IfStmt:
        self.advance()  # if
        condition = self.parse_expression()
        self.expect(TokenType.COLON, "`if` must end with `:`")
        self.expect(TokenType.NEWLINE, "a line ending is required after `if`")
        body = self.parse_block()

        elif_blocks = []
        while self.match(TokenType.ELIF):
            elif_condition = self.parse_expression()
            self.expect(TokenType.COLON, "`elif` must end with `:`")
            self.expect(TokenType.NEWLINE, "a line ending is required after `elif`")
            elif_body = self.parse_block()
            elif_blocks.append((elif_condition, elif_body))

        else_body = None
        if self.match(TokenType.ELSE):
            self.expect(TokenType.COLON, "`else` must end with `:`")
            self.expect(TokenType.NEWLINE, "a line ending is required after `else`")
            else_body = self.parse_block()

        return IfStmt(condition, body, elif_blocks, else_body)

    def parse_print_cmd(self) -> PrintCmdStmt:
        self.advance()  # print_cmd
        self.expect(
            TokenType.LPAREN,
            "`(` is required after `print_cmd`"
        )
        expression = self.parse_expression()
        self.expect(
            TokenType.RPAREN,
            "missing `)` in the `print_cmd` call"
        )
        self.expect(
            TokenType.NEWLINE,
            "a line ending is required after `print_cmd`"
        )
        return PrintCmdStmt(expression)


    def parse_return(self) -> ReturnStmt:
        self.advance()
        if self.at(TokenType.NEWLINE):
            self.advance()
            return ReturnStmt(None)

        value = self.parse_expression()
        self.expect(TokenType.NEWLINE, "a line ending is required after `return`")
        return ReturnStmt(value)

    def parse_assignment_or_expression(self):
        expression = self.parse_expression()

        if isinstance(expression, Name) and self.current().type in self.ASSIGNMENT_TOKENS:
            operator = self.ASSIGNMENT_TOKENS[self.advance().type]
            value = self.parse_expression()
            self.expect(TokenType.NEWLINE, "a line ending is required after the assignment")
            return Assignment(expression.name, operator, value)

        self.expect(TokenType.NEWLINE, "a line ending is required after the expression")
        return ExpressionStmt(expression)

    # Expression precedence:
    # comparison
    # addition/subtraction
    # multiplication/division/modulo
    # unary
    # call / primary

    def parse_expression(self):
        return self.parse_comparison()

    def parse_comparison(self):
        expr = self.parse_term()

        while self.current().type in {
            TokenType.EQ, TokenType.NE, TokenType.GT, TokenType.LT,
            TokenType.GE, TokenType.LE,
        }:
            op = self.advance().value
            right = self.parse_term()
            expr = BinaryExpr(expr, op, right)

        return expr

    def parse_term(self):
        expr = self.parse_factor()

        while self.current().type in (TokenType.PLUS, TokenType.MINUS):
            op = self.advance().value
            right = self.parse_factor()
            expr = BinaryExpr(expr, op, right)

        return expr

    def parse_factor(self):
        expr = self.parse_unary()

        while self.current().type in (
            TokenType.STAR, TokenType.SLASH, TokenType.PERCENT
        ):
            op = self.advance().value
            right = self.parse_unary()
            expr = BinaryExpr(expr, op, right)

        return expr

    def parse_unary(self):
        if self.match(TokenType.PLUS):
            return UnaryExpr("+", self.parse_unary())
        if self.match(TokenType.MINUS):
            return UnaryExpr("-", self.parse_unary())
        return self.parse_call()

    def parse_call(self):
        expr = self.parse_primary()

        while True:
            if self.match(TokenType.LPAREN):
                arguments = []
                if not self.at(TokenType.RPAREN):
                    while True:
                        arguments.append(self.parse_expression())
                        if not self.match(TokenType.COMMA):
                            break
                self.expect(TokenType.RPAREN, "missing `)` in the call")
                expr = CallExpr(expr, arguments)
                continue

            if self.match(TokenType.LBRACKET):
                if not isinstance(expr, Name):
                    self.error(
                        self.current(),
                        "module constant access requires a module name before `[`"
                    )

                if self.current().type is not TokenType.IDENTIFIER:
                    self.error(
                        self.current(),
                        "a constant name is required inside `[]`"
                    )

                constant = self.advance()
                self.expect(
                    TokenType.RBRACKET,
                    "missing `]` in module constant access"
                )
                expr = ModuleConstantAccessExpr(expr.name, constant.value)
                continue

            if self.match(TokenType.DOT):
                if self.current().type in (TokenType.IDENTIFIER, TokenType.TYPE_STRING):
                    method = self.advance()
                else:
                    self.error(
                        self.current(),
                        "a member name is required after `.`"
                    )

                if method.value not in {"type", "string"}:
                    if not isinstance(expr, Name):
                        self.error(
                            method,
                            "module access requires a module name before `.`"
                        )
                    expr = ModuleAccessExpr(expr.name, method.value)
                    continue

                self.expect(
                    TokenType.LPAREN,
                    f"`(` is required after `.{method.value}`"
                )

                if method.value == "string":
                    if not self.match(TokenType.RPAREN):
                        self.error(self.current(), "string() receives no arguments")
                    expr = TypeConversionExpr(expr, "string")
                    continue

                target_types = {
                    TokenType.TYPE_INT: "int",
                    TokenType.TYPE_FLOAT: "float",
                    TokenType.TYPE_STRING: "string",
                    TokenType.TYPE_BOOL: "bool",
                }

                # type() with no argument queries the caller's current type.
                if self.match(TokenType.RPAREN):
                    expr = TypeConversionExpr(expr, None)
                    continue

                target = self.current()
                if target.type not in target_types:
                    self.error(
                        target,
                        "type() requires one of: int, float, string, bool"
                    )

                self.advance()
                target_type = target_types[target.type]

                if not self.at(TokenType.RPAREN):
                    self.error(
                        self.current(),
                        "type() receives only 1 argument"
                    )

                self.advance()
                expr = TypeConversionExpr(expr, target_type)
                continue

            return expr

    def parse_primary(self):
        token = self.current()

        if token.type is TokenType.LPAREN:
            self.advance()
            expression = self.parse_expression()
            self.expect(TokenType.RPAREN, "missing `)` in the parenthesized expression")
            return expression

        if token.type is TokenType.INTEGER:
            self.advance()
            return Literal(token.value)

        if token.type is TokenType.TRUE:
            self.advance()
            return Literal(True)

        if token.type is TokenType.FALSE:
            self.advance()
            return Literal(False)

        if token.type is TokenType.FLOAT:
            self.advance()
            return Literal(token.value)

        if token.type is TokenType.STRING:
            self.advance()
            return Literal(token.value)

        if token.type is TokenType.IDENTIFIER:
            self.advance()
            return Name(token.value)

        if token.type is TokenType.START:
            self.advance()
            return Name("start")

        if token.type is TokenType.PROCESS:
            self.advance()
            return Name("process")

        if token.type in self.TYPE_TOKENS:
            self.error(
                token,
                "a type name is not an expression here"
            )

        self.error(token, "a valid expression was expected")
        raise AssertionError("unreachable")


def ast_to_dict(node):
    """Small debug helper: convert the AST into an easily printable JSON-like dict."""
    if is_dataclass(node):
        result = {}
        for key, value in asdict(node).items():
            result[key] = value
        return result
    return node
