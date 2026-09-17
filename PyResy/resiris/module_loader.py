from __future__ import annotations

import importlib.util
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ModuleInfo:
    """Python-side prototype metadata for a loaded Resiris module."""

    name: str
    functions: str
    variables: str


@dataclass
class ModuleObject:
    """A module-owned object value returned by a module function."""

    module_name: str
    handle: object

    def __repr__(self):
        return f"ModuleObject({self.module_name})"


class ModuleLoader:
    """Minimal Python-side Resiris module loader prototype."""

    def __init__(self, modules_dir: Path | str = "Modules"):
        self.modules_dir = Path(modules_dir)
        self.loaded: dict[str, object] = {}
        self.info: dict[str, ModuleInfo] = {}

    def load(self, module_name: str):
        if module_name in self.loaded:
            raise RuntimeErrorResirisModule(
                f'{module_name} is already included. Error code:"SameModuleMultiCall"'
            )

        module_path = self.modules_dir / f"{module_name}.py"
        if not module_path.is_file():
            raise RuntimeErrorResirisModule(
                f'{module_name} not found! Error code:"MissingModule"'
            )

        internal_name = f"_resiris_module_{module_name}"
        spec = importlib.util.spec_from_file_location(internal_name, module_path)
        if spec is None or spec.loader is None:
            raise RuntimeErrorResirisModule(
                f"{module_name}: module could not be loaded"
            )

        module = importlib.util.module_from_spec(spec)
        sys.modules[internal_name] = module
        try:
            spec.loader.exec_module(module)
            init = getattr(module, "_INIT_", None)
            if not callable(init):
                raise RuntimeErrorResirisModule(
                    f"{module_name}: _INIT_ function is required"
                )
            init()

            name = self._read_string_constant(module, module_name, "NAME")
            functions = self._read_string_constant(module, module_name, "FUNCTIONS")
            variables = self._read_string_constant(module, module_name, "VARIABLES")

            if name != module_name:
                raise RuntimeErrorResirisModule(
                    f'{module_name}: NAME constant must be "{module_name}"'
                )

            self.info[module_name] = ModuleInfo(
                name=name,
                functions=functions,
                variables=variables,
            )
        except Exception:
            self.info.pop(module_name, None)
            sys.modules.pop(internal_name, None)
            raise

        self.loaded[module_name] = module
        return module

    def get(self, module_name: str):
        module = self.loaded.get(module_name)
        if module is None:
            raise RuntimeErrorResirisModule(
                f'{module_name}: module is not included'
            )
        return module

    def has_function(self, module_name: str, function_name: str) -> bool:
        module = self.get(module_name)
        exported = self._csv_names(self.info[module_name].functions)
        return function_name in exported and callable(getattr(module, function_name, None))

    def call_function(self, module_name: str, function_name: str, arguments: list[object]):
        module = self.get(module_name)

        if not self.has_function(module_name, function_name):
            raise RuntimeErrorResirisModule(
                f'{module_name}.{function_name}: unknown module function'
            )

        function = getattr(module, function_name)
        return self._call(module, module_name, function_name, function, arguments)

    def call_object_method(
        self,
        module_name: str,
        handle: object,
        method_name: str,
        arguments: list[object],
    ):
        module = self.get(module_name)

        if not self.has_function(module_name, method_name):
            raise RuntimeErrorResirisModule(
                f'{module_name}.{method_name}: unknown module function'
            )

        method = getattr(module, method_name)
        return self._call(
            module,
            module_name,
            method_name,
            method,
            arguments,
            handle=handle,
        )

    @staticmethod
    def _validate_arguments(module_name: str, function_name: str, arguments: list[object]):
        for argument in arguments:
            if isinstance(argument, bool):
                continue
            if not isinstance(argument, (int, float, str)):
                raise RuntimeErrorResirisModule(
                    f'{argument!r} is not a usable modules argument! Error code:"UnknownModuleArgument"'
                )

    def _call(
        self,
        module,
        module_name: str,
        function_name: str,
        function,
        arguments: list[object],
        handle: object = None,
    ):
        self._validate_arguments(module_name, function_name, arguments)

        all_arguments = [handle] if handle is not None else []
        all_arguments.extend(arguments)

        try:
            result = function(*all_arguments)
        except TypeError as error:
            raise RuntimeErrorResirisModule(
                f'{module_name}.{function_name}: invalid argument count or module function arguments'
            ) from error

        if isinstance(result, (bool, int, float, str)):
            return result

        if result is None:
            raise RuntimeErrorResirisModule(
                f'{module_name}.{function_name}: module returned an unsupported value'
            )

        return ModuleObject(module_name, result)

    def get_constant(self, module_name: str, constant_name: str):
        module = self.get(module_name)
        info = self.info[module_name]

        if constant_name in {"NAME", "FUNCTIONS", "VARIABLES"}:
            return getattr(info, constant_name.lower())

        variable_types = self._variable_descriptions(info.variables)

        if constant_name not in variable_types:
            raise RuntimeErrorResirisModule(
                f'{module_name}.{constant_name}: unknown module constant'
            )

        if not hasattr(module, constant_name):
            raise RuntimeErrorResirisModule(
                f'{module_name}.{constant_name}: exported module constant is missing'
            )

        value = getattr(module, constant_name)
        expected_type = variable_types[constant_name]
        actual_type = self._python_type_name(value)

        if expected_type != actual_type:
            raise RuntimeErrorResirisModule(
                f'{module_name}.{constant_name}: expected {expected_type}, received {actual_type}'
            )

        return value

    @staticmethod
    def _python_type_name(value) -> str:
        if isinstance(value, bool):
            return "bool"
        if isinstance(value, int):
            return "int"
        if isinstance(value, float):
            return "float"
        if isinstance(value, str):
            return "string"
        raise RuntimeErrorResirisModule(
            f'unsupported module value type: {type(value).__name__}'
        )

    @staticmethod
    def _variable_descriptions(value: str) -> dict[str, str]:
        result = {}
        for item in value.split(","):
            item = item.strip()
            if not item:
                continue
            if ":" not in item:
                continue
            name, type_name = (part.strip() for part in item.split(":", 1))
            if name and type_name:
                result[name] = type_name
        return result

    @staticmethod
    def _csv_names(value: str) -> list[str]:
        return [item.strip() for item in value.split(",") if item.strip()]

    @staticmethod
    def _read_string_constant(module, module_name: str, constant_name: str) -> str:
        if not hasattr(module, constant_name):
            raise RuntimeErrorResirisModule(
                f'{module_name}: required constant "{constant_name}" is missing'
            )

        value = getattr(module, constant_name)
        if not isinstance(value, str):
            raise RuntimeErrorResirisModule(
                f'{module_name}: "{constant_name}" must be a string'
            )

        return value


class RuntimeErrorResirisModule(Exception):
    """Internal module-loading error; converted by the Interpreter."""
