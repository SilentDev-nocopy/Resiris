from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import time

from .ast_nodes import (
    Assignment,
    BinaryExpr,
    CallExpr, TypeConversionExpr,
    Declaration,
    ExpressionStmt,
    FunctionDef,
    IfStmt,
    PrintCmdStmt,
    Literal,
    Name,
    Program,
    PassStmt,
    ReturnStmt,
    FunctionalObjectDef,
    UnaryExpr,
    Include,
    MatStmt,
    ModuleAccessExpr,
    ObjectAccessExpr,
    ModuleConstantAccessExpr,
    LifecycleDef,
)
from .tokenizer import ResirisSyntaxError
from .module_loader import ModuleLoader, ModuleObject, RuntimeErrorResirisModule


class RuntimeErrorResiris(Exception):
    """Resiris runtime error."""


class IntDivisionError(RuntimeErrorResiris):
    """Raised when an int value is used as the left side of division."""
    pass


class UnknownVariableError(RuntimeErrorResiris):
    pass


class ResirisTypeError(RuntimeErrorResiris):
    pass


class ConstantAssignmentError(RuntimeErrorResiris):
    pass


class MissingValueError(RuntimeErrorResiris):
    pass


class FunctionError(RuntimeErrorResiris):
    pass


class ReturnSignal(Exception):
    """Internal signal used to execute return."""

    def __init__(self, value):
        super().__init__()
        self.value = value


@dataclass
class FunctionalObject:
    parameters: list[str]
    body: list[object]


@dataclass
class Variable:
    value: object
    type_name: str
    is_constant: bool = False


class Interpreter:
    """
    Resiris PC-side interpreter prototype.

    Currently executes:
      - v / c declarations
        - int / float / string / bool
        - UnknownObject
        - =, +=, -=, *=, /=
        - +, -, *, /, %
        - ==, !=, >, <, >=, <=
      - variable names
      - literals
      - if / elif / else
      - fn
      - return
      - function calls
        - type()
        - str()
        - print_cmd()
      - include / module calls
      - PROCESS / FPS frames

    Scope:
    - function parameters and v variables created inside functions are local
    - global v variables can be read from functions
    - this scope rule is currently a PROTOTYPE, not the final Resiris specification
    """

    def __init__(self, modules_dir: Path | str = "Modules"):
        self.variables: dict[str, Variable] = {}
        self.functions: dict[str, FunctionDef] = {}
        self.start_lifecycle: LifecycleDef | None = None
        self.process_lifecycle: LifecycleDef | None = None
        self.scope_stack: list[dict[str, Variable]] = []
        self.module_loader = ModuleLoader(modules_dir)
        self.modules: dict[str, object] = self.module_loader.loaded
        self._frame_time = 0.0

    def run(self, program: Program) -> dict[str, Variable]:
        # Register normal functions and lifecycle definitions before executing
        # top-level statements. This keeps function/lifecycle definitions
        # independent from their position in the source file.
        top_level_declaration_names = {
            statement.name
            for statement in program.statements
            if isinstance(statement, Declaration)
        }

        for statement in program.statements:
            if isinstance(statement, FunctionDef):
                if statement.name in top_level_declaration_names:
                    raise FunctionError(
                        f"{statement.name}: the name is already used as a variable"
                    )
                self.register_function(statement)
            elif isinstance(statement, LifecycleDef):
                self.register_lifecycle(statement)

        # Execute ordinary top-level code first. Lifecycle bodies are not part
        # of ordinary top-level execution.
        for statement in program.statements:
            if isinstance(statement, (FunctionDef, LifecycleDef)):
                continue
            self.execute(statement)

        # START() is the program entry point and runs once.
        if self.start_lifecycle is not None:
            self.execute_start()

        return self.variables

    def register_lifecycle(self, statement: LifecycleDef):
        if statement.name == "START":
            if self.start_lifecycle is not None:
                raise FunctionError("START: the lifecycle already exists")
            self.start_lifecycle = statement
            return

        if statement.name == "PROCESS":
            if self.process_lifecycle is not None:
                raise FunctionError("PROCESS: the lifecycle already exists")
            self.process_lifecycle = statement
            return

        raise FunctionError(f"{statement.name}: unknown lifecycle")

    def execute_start(self):
        lifecycle = self.start_lifecycle
        if lifecycle is None:
            return
        self._execute_lifecycle(lifecycle, None)

    def _get_process_fps(self) -> float:
        variable = self.variables.get("FPS")
        if variable is None:
            raise RuntimeErrorResiris(
                "PROCESS(FPS) requires a global constant `c FPS float`"
            )
        if not variable.is_constant or variable.type_name != "float":
            raise ResirisTypeError(
                "PROCESS(FPS) requires `FPS` to be a global float constant"
            )
        return variable.value

    def _execute_lifecycle(self, lifecycle: LifecycleDef, fps: float | None):
        local_scope: dict[str, Variable] = {}
        if lifecycle.name == "PROCESS":
            if fps is None:
                raise RuntimeErrorResiris("PROCESS(FPS) requires an FPS value")
            local_scope["FPS"] = Variable(
                value=fps,
                type_name="float",
                is_constant=False,
            )

        self.scope_stack.append(local_scope)
        try:
            self.execute_block(lifecycle.body)
        except ReturnSignal:
            return
        finally:
            self.scope_stack.pop()

    def run_process_frames(self, frame_count: int) -> None:
        if frame_count < 0:
            raise ValueError("frame_count must not be negative")
        lifecycle = self.process_lifecycle
        if lifecycle is None:
            return

        fps = self._get_process_fps()
        if fps <= 0.0:
            raise ResirisTypeError("FPS must be greater than 0.0")

        for _ in range(frame_count):
            self._frame_time += 1.0 / fps
            self._execute_lifecycle(lifecycle, fps)

    def run_process_forever(self) -> None:
        lifecycle = self.process_lifecycle
        if lifecycle is None:
            return

        fps = self._get_process_fps()
        if fps <= 0.0:
            raise ResirisTypeError("FPS must be greater than 0.0")

        frame_period = 1.0 / fps
        next_frame = time.monotonic()

        while True:
            self._frame_time += frame_period
            self._execute_lifecycle(lifecycle, fps)
            next_frame += frame_period
            sleep_time = next_frame - time.monotonic()
            if sleep_time > 0.0:
                time.sleep(sleep_time)
            else:
                # PROCESS took longer than one frame period. Start the next
                # frame immediately instead of accumulating sleep delay.
                next_frame = time.monotonic()

    def register_function(self, statement: FunctionDef):
        if statement.name in self.functions:
            raise FunctionError(
                f"{statement.name}: the function already exists"
            )

        if statement.name in self.variables:
            raise FunctionError(
                f"{statement.name}: the name is already used as a variable"
            )

        self.functions[statement.name] = statement

    def execute(self, statement):
        try:
            if isinstance(statement, Include):
                for module_name in statement.modules:
                    self.module_loader.load(module_name)
                return

            if isinstance(statement, Declaration):
                self.execute_declaration(statement)
                return

            if isinstance(statement, Assignment):
                self.execute_assignment(statement)
                return

            if isinstance(statement, IfStmt):
                self.execute_if(statement)
                return

            if isinstance(statement, MatStmt):
                self.execute_mat(statement)
                return

            if isinstance(statement, (FunctionDef, LifecycleDef)):
                return

            if isinstance(statement, ReturnStmt):
                self.execute_return(statement)
                return

            if isinstance(statement, PassStmt):
                return

            if isinstance(statement, PrintCmdStmt):
                self.execute_print_cmd(statement)
                return

            if isinstance(statement, ExpressionStmt):
                self.evaluate(statement.expression)
                return

            raise RuntimeErrorResiris(
                f"The current interpreter version does not support: "
                f"{type(statement).__name__}"
            )
        except RuntimeErrorResirisModule as error:
            line = getattr(statement, "source_line", None)
            column = getattr(statement, "source_column", None)
            message = str(error)
            if line is not None:
                location = f"line {line}"
                if column is not None:
                    location += f", column {column}"
                message = f"{location}: {message}"
            raise RuntimeErrorResiris(message) from error
        except RuntimeErrorResiris as error:
            line = getattr(statement, "source_line", None)
            column = getattr(statement, "source_column", None)
            if line is not None:
                location = f"line {line}"
                if column is not None:
                    location += f", column {column}"
                raise type(error)(f"{location}: {error}") from error
            raise

    def execute_declaration(self, statement: Declaration):
        current_scope = self.current_scope()

        if statement.name in current_scope:
            raise RuntimeErrorResiris(
                f"{statement.name}: the name is already in use"
            )

        if statement.value is None:
            if statement.type_name == "UnknownObject":
                raise MissingValueError(
                    f"{statement.name}: the first assignment of UnknownObject is required"
                )

            raise MissingValueError(
                f"{statement.name}: a value is currently required for the declaration"
            )

        if statement.type_name == "FunctionalObject":
            if not isinstance(statement.value, FunctionalObjectDef):
                raise ResirisTypeError(
                    f"{statement.name}: FunctionalObject.new(...) is required"
                )
            value = FunctionalObject(statement.value.parameters, statement.value.body)
        else:
            value = self.evaluate(statement.value)

        actual_type = statement.type_name

        if statement.type_name == "UnknownObject":
            actual_type = self.infer_type_name(value)

        value = self.validate_and_coerce(
            actual_type,
            value,
            statement.name,
        )

        current_scope[statement.name] = Variable(
            value=value,
            type_name=actual_type,
            is_constant=(statement.kind == "c"),
        )

    def execute_assignment(self, statement: Assignment):
        variable = self.find_variable(statement.target)

        if variable is None:
            raise UnknownVariableError(
                f"{statement.target}: unknown name"
            )

        if variable.is_constant:
            raise ConstantAssignmentError(
                f"{statement.target}: a constant cannot be modified"
            )

        right = self.evaluate(statement.value)

        if statement.operator == "=":
            new_value = right

        elif statement.operator == "+=":
            new_value = self.apply_binary(
                variable.value, "+", right, statement.target
            )

        elif statement.operator == "-=":
            new_value = self.apply_binary(
                variable.value, "-", right, statement.target
            )

        elif statement.operator == "*=":
            new_value = self.apply_binary(
                variable.value, "*", right, statement.target
            )

        elif statement.operator == "/=":
            new_value = self.apply_binary(
                variable.value, "/", right, statement.target
            )

        else:
            raise RuntimeErrorResiris(
                f"Unknown assignment operator: {statement.operator}"
            )

        variable.value = self.validate_and_coerce(
            variable.type_name,
            new_value,
            statement.target,
        )

    def execute_if(self, statement: IfStmt):
        condition = self.evaluate(statement.condition)

        if not isinstance(condition, bool):
            raise ResirisTypeError(
                "the if condition must produce a bool value"
            )

        if condition:
            self.execute_block(statement.body)
            return

        for elif_condition, elif_body in statement.elif_blocks:
            condition = self.evaluate(elif_condition)

            if not isinstance(condition, bool):
                raise ResirisTypeError(
                    "the elif condition must produce a bool value"
                )

            if condition:
                self.execute_block(elif_body)
                return

        if statement.else_body is not None:
            self.execute_block(statement.else_body)

    def execute_mat(self, statement: MatStmt):
        # The checked value is evaluated exactly once.
        value = self.evaluate(statement.value)
        value_type = self.infer_type_name(value)

        for case in statement.cases:
            if case.type_case:
                # `.type()` returns the requested type name as a string.
                if value != case.value:
                    continue
                self.execute_block(case.body)
                return

            case_value = case.value
            if self.infer_type_name(case_value) != value_type:
                continue

            if value == case_value:
                try:
                    self.execute_block(case.body)
                except RuntimeErrorResiris as error:
                    raise RuntimeErrorResiris(
                        f'{error} Error code:"MatchCaseExecutionError"'
                    ) from error
                return

        if statement.else_body is not None:
            self.execute_block(statement.else_body)

    def execute_block(self, statements):
        for statement in statements:
            self.execute(statement)

    def execute_return(self, statement: ReturnStmt):
        if not self.scope_stack:
            raise FunctionError(
                "`return` can only be used inside a function"
            )

        value = None
        if statement.value is not None:
            value = self.evaluate(statement.value)

        raise ReturnSignal(value)

    def execute_print_cmd(self, statement: PrintCmdStmt):
        value = self.evaluate(statement.expression)
        print(value)

    def call_function(self, function_name: str, arguments: list[object]):
        function = self.functions.get(function_name)

        if function is None:
            raise FunctionError(
                f"{function_name}: unknown function"
            )

        if len(arguments) != len(function.parameters):
            raise FunctionError(
                f"{function_name}: {len(function.parameters)} parameters required, "
                f"but {len(arguments)} arguments received"
            )

        local_scope: dict[str, Variable] = {}

        for parameter_name, argument_value in zip(
            function.parameters,
            arguments,
        ):
            if parameter_name in local_scope:
                raise FunctionError(
                    f"{function_name}: duplicate parameter name: {parameter_name}"
                )

            local_scope[parameter_name] = Variable(
                value=argument_value,
                type_name=self.infer_type_name(argument_value),
                is_constant=False,
            )

        self.scope_stack.append(local_scope)

        try:
            try:
                self.execute_block(function.body)
            except ReturnSignal as signal:
                return signal.value

            # The current prototype result of a function without return is None.
            return None

        finally:
            self.scope_stack.pop()

    def current_scope(self) -> dict[str, Variable]:
        if self.scope_stack:
            return self.scope_stack[-1]

        return self.variables

    def find_variable(self, name: str) -> Variable | None:
        if self.scope_stack:
            local_scope = self.scope_stack[-1]

            if name in local_scope:
                return local_scope[name]

        return self.variables.get(name)

    def evaluate(self, expression):
        if isinstance(expression, Literal):
            return expression.value

        if isinstance(expression, Name):
            variable = self.find_variable(expression.name)

            if variable is None:
                if expression.name in self.functions:
                    return expression.name

                if expression.name in self.modules:
                    return expression.name

                raise UnknownVariableError(
                    f"{expression.name}: unknown name"
                )

            return variable.value

        if isinstance(expression, UnaryExpr):
            value = self.evaluate(expression.operand)

            if expression.operator == "+":
                if not isinstance(value, (int, float)) or isinstance(value, bool):
                    raise ResirisTypeError(
                        f"unary + can only be used with numbers: {value!r}"
                    )

                return +value

            if expression.operator == "-":
                if not isinstance(value, (int, float)) or isinstance(value, bool):
                    raise ResirisTypeError(
                        f"unary - can only be used with numbers: {value!r}"
                    )

                return -value

            raise RuntimeErrorResiris(
                f"Unknown unary operator: {expression.operator}"
            )

        if isinstance(expression, BinaryExpr):
            left = self.evaluate(expression.left)
            right = self.evaluate(expression.right)

            return self.apply_binary(
                left,
                expression.operator,
                right,
                None,
            )

        if isinstance(expression, TypeConversionExpr):
            value = self.evaluate(expression.value)
            return self.convert_type(value, expression.target_type)

        if isinstance(expression, CallExpr):
            arguments = [
                self.evaluate(argument)
                for argument in expression.arguments
            ]

            if isinstance(expression.function, ModuleAccessExpr):
                module_name = expression.function.module_name
                variable = self.find_variable(module_name)
                if variable is not None and isinstance(variable.value, ModuleObject):
                    return self.call_module_object_method(
                        variable.value,
                        expression.function.member_name,
                        arguments,
                    )
                return self.module_loader.call_function(
                    module_name,
                    expression.function.member_name,
                    arguments,
                )

            if isinstance(expression.function, ObjectAccessExpr):
                target = self.evaluate(expression.function.target)
                if not isinstance(target, ModuleObject):
                    raise ResirisTypeError(
                        f"object method calls require a ModuleObject value"
                    )
                return self.call_module_object_method(
                    target,
                    expression.function.member_name,
                    arguments,
                )

            if isinstance(expression.function, Name):
                function_name = expression.function.name

                if function_name == "str":
                    if len(arguments) != 1:
                        raise FunctionError(
                            f"str: 1 argument required, but {len(arguments)} arguments received"
                        )
                    return str(arguments[0])

                variable = self.find_variable(function_name)
                if variable is not None and isinstance(variable.value, FunctionalObject):
                    return self.call_function_object(variable.value, arguments)

                return self.call_function(function_name, arguments)

            raise FunctionError(
                "the function call target must currently be a name or FunctionalObject"
            )

        if isinstance(expression, ModuleAccessExpr):
            variable = self.find_variable(expression.module_name)
            if variable is not None and isinstance(variable.value, ModuleObject):
                return self.call_module_object_method(
                    variable.value,
                    expression.member_name,
                    [],
                )

            if expression.module_name in self.modules:
                try:
                    if self.module_loader.has_function(
                        expression.module_name,
                        expression.member_name,
                    ):
                        return self.module_loader.call_function(
                            expression.module_name,
                            expression.member_name,
                            [],
                        )
                except RuntimeErrorResirisModule:
                    pass

            raise RuntimeErrorResiris(
                "module `.` access is only valid for function calls"
            )

        if isinstance(expression, ObjectAccessExpr):
            target = self.evaluate(expression.target)
            if not isinstance(target, ModuleObject):
                raise ResirisTypeError(
                    f"object member access requires a ModuleObject value"
                )
            return self.call_module_object_method(
                target,
                expression.member_name,
                [],
            )

        if isinstance(expression, ModuleConstantAccessExpr):
            try:
                return self.module_loader.get_constant(
                    expression.module_name,
                    expression.constant_name,
                )
            except RuntimeErrorResirisModule as error:
                raise RuntimeErrorResiris(str(error)) from error


        raise RuntimeErrorResiris(
            f"The current interpreter version does not recognize this expression: "
            f"{type(expression).__name__}"
        )


    def call_function_object(self, function: FunctionalObject, arguments: list[object]):
        if len(arguments) != len(function.parameters):
            raise FunctionError(
                    f"FunctionalObject: {len(function.parameters)} parameters required, "
                    f"but {len(arguments)} arguments received"
            )

        local_scope: dict[str, Variable] = {}
        for parameter_name, argument_value in zip(function.parameters, arguments):
            if parameter_name in local_scope:
                raise FunctionError(
                    f"FunctionalObject: duplicate parameter name: {parameter_name}"
                )
            local_scope[parameter_name] = Variable(
                value=argument_value,
                type_name=self.infer_type_name(argument_value),
                is_constant=False,
            )

        self.scope_stack.append(local_scope)
        try:
            try:
                self.execute_block(function.body)
            except ReturnSignal as signal:
                return signal.value
            return None
        finally:
            self.scope_stack.pop()

    def call_module_object_method(
        self,
        module_object: ModuleObject,
        method_name: str,
        arguments: list[object],
    ):
        handle = module_object.handle

        if isinstance(handle, dict):
            handle["_now"] = self._frame_time

        result = self.module_loader.call_object_method(
            module_object.module_name,
            handle,
            method_name,
            arguments,
        )

        if isinstance(handle, dict):
            pending_callback = handle.pop("_fire_callback", None)
            if pending_callback is not None:
                self.call_function(pending_callback, [])

        return result

    def apply_binary(self, left, operator, right, target_name):
        left_is_number = (
            isinstance(left, (int, float))
            and not isinstance(left, bool)
        )

        right_is_number = (
            isinstance(right, (int, float))
            and not isinstance(right, bool)
        )

        if operator == "+":
            if isinstance(left, str) and isinstance(right, str):
                return left + right

            if not (left_is_number and right_is_number):
                raise ResirisTypeError(
                    f"+: strings of the same type or numeric operands are required; "
                    f"received: {type(left).__name__}, {type(right).__name__}"
                )

            return left + right

        if operator in {"-", "*", "/", "%"}:
            if not (left_is_number and right_is_number):
                raise ResirisTypeError(
                    f"{operator}: numeric operands are required; "
                    f"received: {type(left).__name__}, "
                    f"{type(right).__name__}"
                )

            if operator == "-":
                return left - right

            if operator == "*":
                return left * right

            if operator == "/":
                if right == 0:
                    raise RuntimeErrorResiris("division by zero")

                if isinstance(left, int) and not isinstance(left, bool):
                    raise IntDivisionError(
                        'Cant division with int type! Must use float! Error code:"IntDivisionError"'
                    )

                return left / right

            if operator == "%":
                if right == 0:
                    raise RuntimeErrorResiris("modulo by zero")

                return left % right

        if operator in {"==", "!=", ">", "<", ">=", "<="}:
            if operator == "==":
                return left == right

            if operator == "!=":
                return left != right

            if operator == ">":
                return left > right

            if operator == "<":
                return left < right

            if operator == ">=":
                return left >= right

            if operator == "<=":
                return left <= right

        raise RuntimeErrorResiris(
            f"Unknown binary operator: {operator}"
        )

    def convert_type(self, value, target_type):
        # type() with no argument returns the current type of the caller.
        if target_type is None:
            return self.infer_type_name(value)

        source_type = self.infer_type_name(value)

        if target_type == source_type:
            return value

        if target_type == "int":
            if source_type == "bool":
                return 1 if value else 0
            if source_type == "float":
                # Resiris rounds .5 upwards for positive values.
                return int(value + 0.5)
            if source_type == "int":
                return value
            raise RuntimeErrorResiris(
                f'{source_type} cannot convert to int. Error code:"ConversionFail"'
            )

        if target_type == "float":
            if source_type == "bool":
                return 1.0 if value else 0.0
            if source_type == "int":
                return float(value)
            if source_type == "float":
                return value
            raise RuntimeErrorResiris(
                f'{source_type} cannot convert to float. Error code:"ConversionFail"'
            )

        if target_type == "string":
            if source_type == "bool":
                return "true" if value else "false"
            if source_type in ("int", "float"):
                return str(value)
            if source_type == "string":
                return value
            raise RuntimeErrorResiris(
                f'{source_type} cannot convert to string. Error code:"ConversionFail"'
            )

        if target_type == "bool":
            if source_type == "bool":
                return value
            if source_type in ("int", "float"):
                return value > 0
            raise RuntimeErrorResiris(
                f'{source_type} cannot convert to bool. Error code:"ConversionFail"'
            )

        raise ResirisTypeError(
            f'{target_type} is an invalid type. Error code:"TypeError"'
        )

    def infer_type_name(self, value):
        if isinstance(value, bool):
            return "bool"

        if isinstance(value, int):
            return "int"

        if isinstance(value, float):
            return "float"

        if isinstance(value, str):
            return "string"

        if isinstance(value, ModuleObject):
            return "ModuleObject"

        raise ResirisTypeError(
            f"UnknownObject: type cannot be determined: "
            f"{type(value).__name__}"
        )

    def validate_and_coerce(self, type_name, value, name):
        if type_name == "UnknownObject":
            return value

        if type_name == "int":
            if isinstance(value, bool) or not isinstance(value, int):
                raise ResirisTypeError(
                    f"{name}: an int value is required, "
                    f"received: {type(value).__name__}"
                )

            return value

        if type_name == "float":
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise ResirisTypeError(
                    f"{name}: a float value is required, "
                    f"received: {type(value).__name__}"
                )

            return float(value)

        if type_name == "string":
            if not isinstance(value, str):
                raise ResirisTypeError(
                    f"{name}: a string value is required, "
                    f"received: {type(value).__name__}"
                )

            return value

        if type_name == "bool":
            if not isinstance(value, bool):
                raise ResirisTypeError(
                    f"{name}: a bool value is required, "
                    f"received: {type(value).__name__}"
                )

            return value

        if type_name == "FunctionalObject":
            if not isinstance(value, FunctionalObject):
                raise ResirisTypeError(
                    f"{name}: a FunctionalObject value is required, received: {type(value).__name__}"
                )
            return value

        if type_name == "ModuleObject":
            if not isinstance(value, ModuleObject):
                raise ResirisTypeError(
                    f"{name}: a ModuleObject value is required, "
                    f"received: {type(value).__name__}"
                )
            return value

        raise RuntimeErrorResiris(
            f"{name}: unknown type: {type_name}"
        )
