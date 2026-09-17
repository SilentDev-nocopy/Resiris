from pathlib import Path

import pytest

from resiris.ast_nodes import CallExpr, ModuleAccessExpr, ObjectAccessExpr
from resiris.interpreter import Interpreter, RuntimeErrorResiris
from resiris.module_loader import ModuleObject
from resiris.parser import Parser
from resiris.tokenizer import Tokenizer


def parse(source: str):
    return Parser(Tokenizer().tokenize(source)).parse()


def static_interpreter():
    project_root = Path(__file__).resolve().parents[1]
    return Interpreter(modules_dir=project_root / "Modules")


def test_rsbase_await_creates_module_object():
    interpreter = static_interpreter()
    interpreter.run(parse(
        "<include> RSBase\n"
        "v timer ModuleObject = RSBase.await\n"
    ))
    variable = interpreter.variables["timer"]
    assert variable.type_name == "ModuleObject"
    assert isinstance(variable.value, ModuleObject)
    assert variable.value.module_name == "RSBase"


def test_print_cmd_can_print_module_name(capsys):
    interpreter = static_interpreter()
    interpreter.run(parse(
        "<include> RSBase\n"
        "START():\n"
        "\tprint_cmd(RSBase)\n"
    ))
    assert capsys.readouterr().out == "RSBase\n"


def test_timer_set_timer_channels_and_distributes(capsys):
    source = (
        "<include> RSBase\n"
        "fn on_done():\n"
        "\tprint_cmd(\"done\")\n"
        "c FPS float = 10.0\n"
        "v timer ModuleObject = RSBase.await\n"
        "START():\n"
        "\ttimer.set_timer(0.3).on_timeout(on_done)\n"
        "PROCESS(FPS):\n"
        "\tprint_cmd(timer.process)\n"
    )
    interpreter = static_interpreter()
    interpreter.run(parse(source))

    interpreter.run_process_frames(3)
    out1 = capsys.readouterr().out
    assert "done\n" in out1
    assert out1.count("done\n") == 1

    interpreter.run_process_frames(3)
    out2 = capsys.readouterr().out
    assert "done\n" not in out2


def test_timer_process_prints_decreasing_remaining(capsys):
    source = (
        "<include> RSBase\n"
        "c FPS float = 10.0\n"
        "v timer ModuleObject = RSBase.await\n"
        "START():\n"
        "\ttimer.set_timer(0.3)\n"
        "PROCESS(FPS):\n"
        "\tprint_cmd(timer.process)\n"
    )
    interpreter = static_interpreter()
    interpreter.run(parse(source))

    interpreter.run_process_frames(2)
    lines = capsys.readouterr().out.strip().splitlines()
    assert len(lines) == 2
    values = [float(line) for line in lines]
    assert values[0] > values[1] >= 0.0


def test_timer_reset_keeps_timer_alive(capsys):
    source = (
        "<include> RSBase\n"
        "fn on_done():\n"
        "\tprint_cmd(\"done\")\n"
        "c FPS float = 10.0\n"
        "v timer ModuleObject = RSBase.await\n"
        "START():\n"
        "\ttimer.set_timer(0.1).on_timeout(on_done)\n"
        "PROCESS(FPS):\n"
        "\ttimer.reset_timer()\n"
        "\ttimer.process\n"
    )
    interpreter = static_interpreter()
    interpreter.run(parse(source))

    interpreter.run_process_frames(10)
    assert capsys.readouterr().out.count("done\n") == 0


def test_timer_stop_prevents_fire(capsys):
    source = (
        "<include> RSBase\n"
        "fn on_done():\n"
        "\tprint_cmd(\"done\")\n"
        "c FPS float = 10.0\n"
        "v timer ModuleObject = RSBase.await\n"
        "START():\n"
        "\ttimer.set_timer(0.1).on_timeout(on_done)\n"
        "\ttimer.stop_timer()\n"
        "PROCESS(FPS):\n"
        "\ttimer.process\n"
    )
    interpreter = static_interpreter()
    interpreter.run(parse(source))

    interpreter.run_process_frames(10)
    assert capsys.readouterr().out.count("done\n") == 0


def test_timer_free_prevents_fire(capsys):
    source = (
        "<include> RSBase\n"
        "fn on_done():\n"
        "\tprint_cmd(\"done\")\n"
        "c FPS float = 10.0\n"
        "v timer ModuleObject = RSBase.await\n"
        "START():\n"
        "\ttimer.set_timer(0.1).on_timeout(on_done)\n"
        "\ttimer.free_timer()\n"
        "PROCESS(FPS):\n"
        "\ttimer.process\n"
    )
    interpreter = static_interpreter()
    interpreter.run(parse(source))

    interpreter.run_process_frames(10)
    assert capsys.readouterr().out.count("done\n") == 0


def test_chained_object_access_ast_shape():
    program = parse("<include> RSBase\ntimer.set_timer(0.3).on_timeout(on_done)\n")
    outer = program.statements[1].expression

    assert isinstance(outer, CallExpr)
    assert isinstance(outer.function, ObjectAccessExpr)
    assert outer.function.member_name == "on_timeout"

    inner = outer.function.target
    assert isinstance(inner, CallExpr)
    assert isinstance(inner.function, ModuleAccessExpr)
    assert inner.function.module_name == "timer"
    assert inner.function.member_name == "set_timer"


def test_module_constant_bare_access_is_still_rejected():
    interpreter = static_interpreter()
    with pytest.raises(RuntimeErrorResiris, match="only valid for function calls"):
        interpreter.run(parse(
            "<include> RSMath\n"
            "v x UnknownObject = RSMath.PI\n"
        ))


def test_chained_object_access_returns_the_object(tmp_path):
    modules = tmp_path / "Modules"
    modules.mkdir()
    (modules / "TestMod.py").write_text(
        """NAME = "TestMod"
FUNCTIONS = "make,set_duration,set_name"
VARIABLES = ""

def _INIT_():
    pass

def make():
    return {"duration": 0.0, "name": ""}

def set_duration(handle, seconds):
    handle["duration"] = float(seconds)
    return handle

def set_name(handle, name):
    handle["name"] = name
    return handle
""",
        encoding="utf-8",
    )
    interpreter = Interpreter(modules_dir=modules)
    interpreter.run(parse(
        "<include> TestMod\n"
        "v obj ModuleObject = TestMod.make\n"
        "obj.set_duration(0.5).set_name(\"fast\")\n"
    ))
    variable = interpreter.variables["obj"]
    handle = variable.value.handle
    assert handle["duration"] == 0.5
    assert handle["name"] == "fast"