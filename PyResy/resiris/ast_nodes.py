from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Program:
    statements: list[object] = field(default_factory=list)


@dataclass
class Include:
    modules: list[str]


@dataclass
class Declaration:
    kind: str                 # "v" or "c"
    name: str
    type_name: str
    value: Optional[object] = None


@dataclass
class FunctionDef:
    name: str
    parameters: list[str]
    body: list[object]


@dataclass
class LifecycleDef:
    name: str
    parameter_name: str | None
    body: list[object]


@dataclass
class IfStmt:
    condition: object
    body: list[object]
    elif_blocks: list[tuple[object, list[object]]] = field(default_factory=list)
    else_body: Optional[list[object]] = None


@dataclass
class ReturnStmt:
    value: object | None = None


@dataclass
class PassStmt:
    pass


@dataclass
class PrintCmdStmt:
    expression: object


@dataclass
class Assignment:
    target: str
    operator: str
    value: object


@dataclass
class ExpressionStmt:
    expression: object


@dataclass
class Literal:
    value: object


@dataclass
class Name:
    name: str


@dataclass
class UnaryExpr:
    operator: str
    operand: object


@dataclass
class BinaryExpr:
    left: object
    operator: str
    right: object


@dataclass
class FunctionalObjectDef:
    parameters: list[str]
    body: list[object]


@dataclass
class CallExpr:
    function: object
    arguments: list[object]


@dataclass
class ModuleAccessExpr:
    module_name: str
    member_name: str


@dataclass
class ObjectAccessExpr:
    target: object
    member_name: str


@dataclass
class ModuleConstantAccessExpr:
    module_name: str
    constant_name: str


@dataclass
class TypeConversionExpr:
    value: object
    target_type: str


@dataclass
class MatCase:
    value: object
    body: list[object]
    type_case: bool = False


@dataclass
class MatStmt:
    value: object
    cases: list[MatCase]
    else_body: Optional[list[object]] = None
