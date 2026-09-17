from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto


class TokenType(Enum):
    # Structure
    NEWLINE = auto()
    INDENT = auto()
    DEDENT = auto()
    EOF = auto()

    # Names / literals
    IDENTIFIER = auto()
    INTEGER = auto()
    FLOAT = auto()
    STRING = auto()

    # Keywords
    INCLUDE = auto()
    V = auto()
    C = auto()
    FN = auto()
    START = auto()
    PROCESS = auto()
    IF = auto()
    ELIF = auto()
    ELSE = auto()
    MAT = auto()
    RETURN = auto()
    PASS = auto()
    PRINT_CMD = auto()
    TRUE = auto()
    FALSE = auto()

    # Types
    TYPE_UNKNOWN = auto()
    TYPE_INT = auto()
    TYPE_FLOAT = auto()
    TYPE_STRING = auto()
    TYPE_BOOL = auto()
    TYPE_MODULE_OBJECT = auto()
    TYPE_FUNCTIONAL_OBJECT = auto()

    # Operators
    ASSIGN = auto()
    PLUS_ASSIGN = auto()
    MINUS_ASSIGN = auto()
    STAR_ASSIGN = auto()
    SLASH_ASSIGN = auto()

    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()

    EQ = auto()
    NE = auto()
    GT = auto()
    LT = auto()
    GE = auto()
    LE = auto()

    # Punctuation
    COLON = auto()
    COMMA = auto()
    LPAREN = auto()
    RPAREN = auto()
    DOT = auto()
    LBRACKET = auto()
    RBRACKET = auto()


KEYWORDS = {
    "include": TokenType.INCLUDE,
    "v": TokenType.V,
    "c": TokenType.C,
    "fn": TokenType.FN,
    "START": TokenType.START,
    "PROCESS": TokenType.PROCESS,
    "if": TokenType.IF,
    "elif": TokenType.ELIF,
    "else": TokenType.ELSE,
    "mat": TokenType.MAT,
    "return": TokenType.RETURN,
    "pass": TokenType.PASS,
    "print_cmd": TokenType.PRINT_CMD,
    "true": TokenType.TRUE,
    "false": TokenType.FALSE,

    "UnknownObject": TokenType.TYPE_UNKNOWN,
    "int": TokenType.TYPE_INT,
    "float": TokenType.TYPE_FLOAT,
    "string": TokenType.TYPE_STRING,
    "bool": TokenType.TYPE_BOOL,
    "ModuleObject": TokenType.TYPE_MODULE_OBJECT,
    "FunctionalObject": TokenType.TYPE_FUNCTIONAL_OBJECT,
}


@dataclass(frozen=True)
class Token:
    type: TokenType
    value: object
    line: int
    column: int

    def __repr__(self) -> str:
        return (
            f"Token({self.type.name}, {self.value!r}, "
            f"{self.line}:{self.column})"
        )


class ResirisSyntaxError(Exception):
    pass


class Tokenizer:
    """
    Resiris tokenizer.

    Current syntax implemented here:
      - ## comments
      - indentation blocks (tabs)
      - v / c declarations
      - fn / START / PROCESS / FunctionalObject / include
      - if / elif / else
      - return / pass / mat / print_cmd / print_cmd
      - arithmetic/comparison/assignment operators
      - int / float / string / bool / UnknownObject / ModuleObject
      - strings in single or double quotes

    `await` is a planned future feature and is not part of the current syntax.
    """

    TWO_CHAR = {
        "+=": TokenType.PLUS_ASSIGN,
        "-=": TokenType.MINUS_ASSIGN,
        "*=": TokenType.STAR_ASSIGN,
        "/=": TokenType.SLASH_ASSIGN,
        "==": TokenType.EQ,
        "!=": TokenType.NE,
        ">=": TokenType.GE,
        "<=": TokenType.LE,
    }

    ONE_CHAR = {
        "=": TokenType.ASSIGN,
        "+": TokenType.PLUS,
        "-": TokenType.MINUS,
        "*": TokenType.STAR,
        "/": TokenType.SLASH,
        "%": TokenType.PERCENT,
        ">": TokenType.GT,
        "<": TokenType.LT,
        ":": TokenType.COLON,
        ",": TokenType.COMMA,
        "(": TokenType.LPAREN,
        ")": TokenType.RPAREN,
        ".": TokenType.DOT,
        "[": TokenType.LBRACKET,
        "]": TokenType.RBRACKET,
    }

    def tokenize(self, source: str) -> list[Token]:
        tokens: list[Token] = []
        indent_stack = [0]
        lines = source.splitlines()

        for line_no, raw_line in enumerate(lines, start=1):
            # Resiris uses TAB characters for indentation.
            # Leading spaces are not valid indentation.
            leading = raw_line[: len(raw_line) - len(raw_line.lstrip(" \t"))]
            if " " in leading:
                raise ResirisSyntaxError(
                    f"line {line_no}: spaces cannot be used for indentation; use tabs"
                )

            stripped = raw_line.lstrip("\t")
            if not stripped or stripped.startswith("##"):
                # Blank/comment-only lines do not create NEWLINE tokens.
                continue

            indent = len(raw_line) - len(stripped)

            if indent > indent_stack[-1]:
                indent_stack.append(indent)
                tokens.append(Token(TokenType.INDENT, indent, line_no, 1))
            elif indent < indent_stack[-1]:
                while indent < indent_stack[-1]:
                    indent_stack.pop()
                    tokens.append(Token(TokenType.DEDENT, indent, line_no, 1))
                if indent != indent_stack[-1]:
                    raise ResirisSyntaxError(
                        f"line {line_no}: invalid indentation"
                    )

            i = indent
            n = len(raw_line)

            while i < n:
                ch = raw_line[i]

                if ch in (" ", "\t"):
                    i += 1
                    continue

                if raw_line.startswith("##", i):
                    break

                column = i + 1

                # Two-character operators first.
                two = raw_line[i:i + 2]
                if two in self.TWO_CHAR:
                    tokens.append(Token(self.TWO_CHAR[two], two, line_no, column))
                    i += 2
                    continue

                if ch in self.ONE_CHAR:
                    tokens.append(Token(self.ONE_CHAR[ch], ch, line_no, column))
                    i += 1
                    continue

                if ch in "\"'":
                    quote = ch
                    start = i
                    i += 1
                    value_chars: list[str] = []

                    while i < n:
                        if raw_line[i] == "\\":
                            if i + 1 >= n:
                                raise ResirisSyntaxError(
                                    f"line {line_no}, column {i+1}: unterminated string"
                                )
                            escaped = raw_line[i + 1]
                            escapes = {
                                "n": "\n",
                                "t": "\t",
                                "\\": "\\",
                                "\"": "\"",
                                "'": "'",
                            }
                            value_chars.append(escapes.get(escaped, escaped))
                            i += 2
                            continue

                        if raw_line[i] == quote:
                            i += 1
                            break

                        value_chars.append(raw_line[i])
                        i += 1
                    else:
                        raise ResirisSyntaxError(
                            f"line {line_no}, column {start+1}: unterminated string"
                        )

                    tokens.append(
                        Token(TokenType.STRING, "".join(value_chars), line_no, start + 1)
                    )
                    continue

                if ch.isdigit():
                    start = i
                    while i < n and raw_line[i].isdigit():
                        i += 1

                    token_type = TokenType.INTEGER
                    value: object = int(raw_line[start:i])

                    if i < n and raw_line[i] == ".":
                        if i + 1 < n and raw_line[i + 1].isdigit():
                            i += 1
                            while i < n and raw_line[i].isdigit():
                                i += 1
                            token_type = TokenType.FLOAT
                            value = float(raw_line[start:i])
                        else:
                            raise ResirisSyntaxError(
                                f"line {line_no}, column {i+1}: "
                                "a digit is required after the decimal point"
                            )

                    tokens.append(Token(token_type, value, line_no, start + 1))
                    continue

                if ch.isalpha() or ch == "_":
                    start = i
                    i += 1
                    while i < n and (raw_line[i].isalnum() or raw_line[i] == "_"):
                        i += 1

                    word = raw_line[start:i]
                    token_type = KEYWORDS.get(word, TokenType.IDENTIFIER)
                    tokens.append(Token(token_type, word, line_no, start + 1))
                    continue

                raise ResirisSyntaxError(
                    f"line {line_no}, column {column}: unknown character: {ch!r}"
                )

            tokens.append(Token(TokenType.NEWLINE, "\\n", line_no, n + 1))

        # Close every open indentation block.
        final_line = max(1, len(lines))
        while len(indent_stack) > 1:
            indent_stack.pop()
            tokens.append(Token(TokenType.DEDENT, indent_stack[-1], final_line, 1))

        tokens.append(Token(TokenType.EOF, None, final_line, 1))
        return tokens
