#!/usr/bin/env python3
"""Derive the extension module registry from the Python module prototypes."""

import re
from pathlib import Path

MODULE_DOCUMENTATION = {
    "RSBase": (
        "Real-time base module for object scheduling: timers (await, set_timer, "
        "on_timeout, reset_timer, stop_timer, free_timer) and per-frame process()."
    ),
    "RSMath": (
        "Math module: trigonometry, powers and roots, rounding, statistics, "
        "number classification and the PI/E constants."
    ),
}


def parse_module_constants(path: Path) -> dict:
    """Extract the NAME/FUNCTIONS/VARIABLES constants from a module prototype."""
    text = path.read_text(encoding="utf-8")

    def extract(const: str) -> str:
        match = re.search(
            r"^" + const + r"\s*=\s*(.*?)(?=^\s*[A-Z][A-Z0-9_]*\s*=|\\Z)",
            text,
            re.MULTILINE | re.DOTALL,
        )
        if not match:
            return ""
        return "".join(re.findall(r'"([^"]*)"', match.group(1)))

    name = extract("NAME").strip()
    functions = [part.strip() for part in extract("FUNCTIONS").split(",") if part.strip()]
    variables = []
    for part in extract("VARIABLES").split(","):
        part = part.strip()
        if ":" in part:
            var_name, var_type = (piece.strip() for piece in part.split(":", 1))
            if var_name and var_type:
                variables.append({"name": var_name, "type": var_type})
    return {"name": name, "functions": functions, "variables": variables}


def generate_modules_metadata(modules_dir: Path) -> dict:
    """Build the extension module registry from the Python module prototypes."""
    modules = []
    if not modules_dir.is_dir():
        return {"version": 1, "modules": modules}
    for module_path in sorted(modules_dir.glob("*.py")):
        try:
            meta = parse_module_constants(module_path)
        except Exception:
            continue
        if not meta["name"]:
            continue
        meta["documentation"] = MODULE_DOCUMENTATION.get(meta["name"], "")
        modules.append(meta)
    return {"version": 1, "modules": modules}
