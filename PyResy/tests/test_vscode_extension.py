import json
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VSCODE = ROOT / "pre_packaging" / "vscode"
VSIX = VSCODE / "build" / "resiris-language-support-0.5.0.vsix"


def test_extension_manifest_declares_static_contributions_only():
    package = json.loads((VSCODE / "package.json").read_text(encoding="utf-8"))

    assert package["name"] == "resiris-language-support"
    assert package["displayName"] == "Resiris Language Support"

    # No runtime code at all: the extension is purely declarative.
    assert "main" not in package
    assert "activationEvents" not in package
    assert "contributes" in package

    languages = package["contributes"]["languages"]
    resiris = next(item for item in languages if item["id"] == "resiris")
    assert ".resy" in resiris["extensions"]

    grammars = package["contributes"]["grammars"]
    assert any(item["language"] == "resiris" for item in grammars)

    assert "snippets" not in package["contributes"]

    themes = package["contributes"]["iconThemes"]
    assert any(item["id"] == "resiris-seti" for item in themes)

    color_themes = package["contributes"]["themes"]
    labels = {item["label"] for item in color_themes}
    assert "Resiris Default" in labels
    assert "Resiris Nord" in labels


def test_language_configuration_forces_tabs():
    config = json.loads(
        (VSCODE / "language-configuration.json").read_text(encoding="utf-8")
    )
    defaults = json.loads((VSCODE / "package.json").read_text(encoding="utf-8"))["contributes"]["configurationDefaults"]

    assert config["comments"]["lineComment"] == "##"
    assert defaults["[resiris]"]["editor.insertSpaces"] is False
    assert defaults["[resiris]"]["editor.detectIndentation"] is False
    assert defaults["[resiris]"]["editor.tabSize"] == 4


def test_language_configuration_keeps_function_body_indented():
    import re

    config = json.loads(
        (VSCODE / "language-configuration.json").read_text(encoding="utf-8")
    )
    rules = config["indentationRules"]

    increase = re.compile(rules["increaseIndentPattern"])
    assert increase.match("START():")
    assert increase.match("PROCESS(FPS):")
    assert increase.match("if x == 5:")

    decrease = rules.get("decreaseIndentPattern")
    if decrease:
        assert not re.compile(decrease).match("\tprint_cmd(FPS)")
        assert not re.compile(decrease).match("\t\tv x int = 0")


def test_grammar_contains_current_resiris_keywords_and_lifecycle():
    grammar = json.loads(
        (VSCODE / "syntaxes" / "resiris.tmLanguage.json").read_text(encoding="utf-8")
    )
    text = json.dumps(grammar)

    for word in ("v", "c", "fn", "START", "PROCESS", "if", "elif", "else", "mat", "return", "pass"):
        assert word in text

    assert "##.*$" in text
    assert "print_cmd" in text
    assert "RSMath" not in text  # modules are intentionally generic


def test_extension_ships_no_runtime_code():
    # The extension is limited to syntax highlighting, themes and icons:
    # all completion/diagnostic logic was removed, so no JS or module
    # registry files may exist.
    for removed in ("extension.js", "language-core.js", "modules.json"):
        assert not (VSCODE / removed).exists(), f"{removed} should have been removed"

    package = json.loads((VSCODE / "package.json").read_text(encoding="utf-8"))
    assert "main" not in package
    assert "activationEvents" not in package


def test_theme_gives_every_resiris_scope_its_own_color():
    grammar = json.loads(
        (VSCODE / "syntaxes" / "resiris.tmLanguage.json").read_text(encoding="utf-8")
    )

    def collect_names(node):
        names = set()
        if isinstance(node, dict):
            if isinstance(node.get("name"), str):
                names.add(node["name"])
            for value in node.values():
                names |= collect_names(value)
        elif isinstance(node, list):
            for item in node:
                names |= collect_names(item)
        return names

    scopes = {s for s in collect_names(grammar) if s.endswith(".resiris")}

    for theme_path in sorted((VSCODE / "themes").glob("*.json")):
        theme = json.loads(theme_path.read_text(encoding="utf-8"))

        colored = {}
        for rule in theme["tokenColors"]:
            scope = rule.get("scope")
            if not scope or not str(scope).endswith(".resiris"):
                continue
            colored[str(scope)] = rule["settings"]["foreground"]

        missing = sorted(scopes - set(colored))
        assert not missing, f"{theme_path.name} missing colors for: {missing}"
        assert len(set(colored.values())) == len(colored), (
            f"{theme_path.name} has duplicate colors among resiris scopes"
        )


def test_built_vsix_contains_language_support_files():
    assert VSIX.is_file()

    with zipfile.ZipFile(VSIX) as archive:
        names = set(archive.namelist())

    expected = {
        "extension/package.json",
        "extension/language-configuration.json",
        "extension/syntaxes/resiris.tmLanguage.json",
        "extension/icons/resiris-seti-icon-theme.json",
        "extension/icons/resy.png",
        "extension/icons/seti.woff",
        "extension/themes/resiris-color-theme.json",
        "extension/themes/resiris-nord-theme.json",
    }
    assert expected <= names
    assert "extension/extension.js" not in names
    assert "extension/language-core.js" not in names
    assert "extension/modules.json" not in names