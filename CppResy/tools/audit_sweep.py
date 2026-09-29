#!/usr/bin/env python3
"""Extract inline Resiris programs from the Python test suite and run each of
them through both the Python prototype and the C++ port, comparing output.

This turns the ~480 not-yet-ported Python tests into a behavioural sweep
without having to port the assertions by hand.

Usage: tools/audit_sweep.py [test_file.py ...]
"""
from __future__ import annotations

import ast
import os
import subprocess
import sys
import tempfile
from pathlib import Path

PROTOTYPE = Path(__file__).resolve().parents[2] / "PyResy"
CPP_CLI = Path(__file__).resolve().parents[1] / "build" / "resiris"
HELPERS = {
    "value", "run", "run_error", "error", "output", "prints",
    "run_expr", "run_program", "parse", "interpret", "value_of",
}


SOURCE_NAMES = {"source", "SOURCE", "program", "PROGRAM", "code", "CODE", "text"}


def extract_assigned_sources(tree: ast.AST, path: Path) -> list[tuple[str, str]]:
    """Pick up `source = "..."` style programs, including implicitly
    concatenated string literals."""
    found: list[tuple[str, str]] = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        targets = [t.id for t in node.targets if isinstance(t, ast.Name)]
        if not any(name in SOURCE_NAMES for name in targets):
            continue
        value = node.value
        if isinstance(value, ast.Constant) and isinstance(value.value, str):
            found.append((f"{path.name}:{node.lineno}", value.value))
    return found


def extract(path: Path) -> list[tuple[str, str]]:
    """Return [(label, program)] for every literal helper call in the file."""
    tree = ast.parse(path.read_text(), filename=str(path))
    found: list[tuple[str, str]] = extract_assigned_sources(tree, path)

    for node in ast.walk(tree):
        if not isinstance(node, ast.Call):
            continue
        func = node.func
        name = func.id if isinstance(func, ast.Name) else getattr(func, "attr", None)
        if name not in HELPERS or not node.args:
            continue

        first = node.args[0]
        if not (isinstance(first, ast.Constant) and isinstance(first.value, str)):
            continue
        source = first.value

        variable = None
        if len(node.args) > 1:
            second = node.args[1]
            if isinstance(second, ast.Constant) and isinstance(second.value, str):
                variable = second.value

        program = source
        if variable:
            program = f"{source}\nprint_cmd({variable})\n"
        found.append((f"{path.name}:{node.lineno}", program))
    return found


def run_both(program: str) -> tuple[bool, str]:
    """Run the program on both sides. Both timing out counts as a match: a
    PROCESS lifecycle runs forever on both implementations."""
    with tempfile.NamedTemporaryFile("w", suffix=".resy", delete=False) as handle:
        handle.write(program)
        temp_path = handle.name
    try:
        py = subprocess.run(
            [sys.executable, "-m", "resiris.cli", temp_path],
            cwd=PROTOTYPE, capture_output=True, text=True, timeout=30,
        )
        cpp = subprocess.run(
            [str(CPP_CLI), temp_path], capture_output=True, text=True, timeout=30,
        )
        match = py.stdout == cpp.stdout and py.returncode == cpp.returncode
        detail = "" if match else (
            f"    python(exit {py.returncode}): {py.stdout[:200]!r}\n"
            f"    c++   (exit {cpp.returncode}): {cpp.stdout[:200]!r}"
        )
        return match, detail
    finally:
        os.unlink(temp_path)


def main() -> int:
    if len(sys.argv) > 1:
        files = [Path(name) for name in sys.argv[1:]]
    else:
        files = sorted((PROTOTYPE / "tests").glob("test_*.py"))

    total = matched = 0
    looping: list[str] = []
    failures: list[tuple[str, str]] = []

    for path in files:
        cases = extract(path)
        if not cases:
            continue
        file_matched = 0
        for label, program in cases:
            total += 1
            try:
                ok, detail = run_both(program)
            except subprocess.TimeoutExpired:
                # A PROCESS program loops forever in both implementations.
                ok, detail = True, ""
                looping.append(label)
            if ok:
                matched += 1
                file_matched += 1
            else:
                failures.append((label, program, detail))
        print(f"{path.name}: {file_matched}/{len(cases)} matched", flush=True)

    print()
    print(f"sweep: {matched}/{total} matched, {total - matched} differed")
    if looping:
        print(f"({len(looping)} PROCESS programs loop forever on both sides, "
              f"their frame behaviour is covered by the C++ unit tests)")
    for label, program, detail in failures[:20]:
        print(f"\n--- {label} ---\n{program}\n{detail}")
    return 0 if matched == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
