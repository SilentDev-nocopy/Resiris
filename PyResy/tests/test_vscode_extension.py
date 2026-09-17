import json
import zipfile
from pathlib import Path

from pre_packaging.vscode.module_metadata import generate_modules_metadata


ROOT = Path(__file__).resolve().parents[1]
VSCODE = ROOT / "pre_packaging" / "vscode"
VSIX = VSCODE / "build" / "resiris-language-support-0.5.0.vsix"


def test_extension_manifest_declares_resy_language_and_icon_theme():
    package = json.loads((VSCODE / "package.json").read_text(encoding="utf-8"))

    assert package["name"] == "resiris-language-support"
    assert package["displayName"] == "Resiris Language Support"

    languages = package["contributes"]["languages"]
    resiris = next(item for item in languages if item["id"] == "resiris")
    assert ".resy" in resiris["extensions"]

    grammars = package["contributes"]["grammars"]
    assert any(item["language"] == "resiris" for item in grammars)

    snippets = package["contributes"]["snippets"]
    assert any(item["language"] == "resiris" for item in snippets)

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

def test_snippets_use_tab_indentation():
    snippets = json.loads(
        (VSCODE / "snippets" / "resiris.json").read_text(encoding="utf-8")
    )

    assert snippets["Resiris function"]["body"][1].startswith("\t")


def test_completion_provider_handles_lifecycle_and_fps():
    source = (VSCODE / "extension.js").read_text(encoding="utf-8")

    assert '"start"' in source
    assert '"process"' in source
    assert "START():" in source
    assert "PROCESS(FPS):" in source
    assert "c FPS float = 30.0" in source
    assert "hasFpsConstant" in source
    assert "registerCompletionItemProvider" in source


def test_extension_reports_missing_includes_and_offers_quick_fix():
    source = (VSCODE / "extension.js").read_text(encoding="utf-8")

    assert "createDiagnosticCollection" in source
    assert "registerCodeActionsProvider" in source
    assert "resiris.missing-include" in source
    assert "Used but not included".lower() in source.lower() or "not included" in source
    assert "Add <include>" in source
    assert "insertIncludesEdit" in source
    assert "DiagnosticSeverity.Warning" in source
    assert "getIncludedModules" in source
    assert "findMissingModuleUses" in source


def test_module_registry_in_sync_with_python_modules():
    registry = json.loads((VSCODE / "modules.json").read_text(encoding="utf-8"))
    generated = generate_modules_metadata(ROOT / "Modules")

    assert registry == generated
    assert registry["version"] == 1

    by_name = {module["name"]: module for module in registry["modules"]}
    assert by_name["RSBase"]["functions"] == [
        "await",
        "set_timer",
        "on_timeout",
        "reset_timer",
        "stop_timer",
        "free_timer",
        "process",
    ]
    assert "sqrt" in by_name["RSMath"]["functions"]
    assert {"name": "PI", "type": "float"} in by_name["RSMath"]["variables"]
    assert by_name["RSMath"]["documentation"]


def test_module_completions_are_context_aware():
    source = (VSCODE / "extension.js").read_text(encoding="utf-8")

    assert "CompletionItemKind.Module" in source
    assert "CompletionItemKind.Function" in source
    assert "CompletionItemKind.Constant" in source
    assert "m.name.toLowerCase().startsWith(prefix" in source
    assert "match.index" in source


def test_completions_are_gated_by_line_context():
    source = (VSCODE / "extension.js").read_text(encoding="utf-8")

    assert "scanLine" in source
    assert "isStatementStart" in source
    assert "accessCompletions" in source
    assert "resolveObjectModule" in source
    assert "expressionItems" in source
    assert "statementItems" in source
    assert "replaceRange" in source
    assert "ModuleObject" in source
    assert "parenthesis" in source.lower() or "parenDepth" in source
    assert "preselect" in source


def test_completion_is_triggered_only_in_context():
    source = (VSCODE / "extension.js").read_text(encoding="utf-8")

    # Letters are deliberately NOT trigger characters: an always-open widget
    # matched every snippet at word boundaries and let a stray Tab expand one.
    assert 'const TRIGGER_CHARS = [".", "[", ">"];' in source


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
        "extension/extension.js",
        "extension/modules.json",
        "extension/language-configuration.json",
        "extension/syntaxes/resiris.tmLanguage.json",
        "extension/snippets/resiris.json",
        "extension/icons/resiris-seti-icon-theme.json",
        "extension/icons/resy.png",
        "extension/icons/seti.woff",
        "extension/themes/resiris-color-theme.json",
        "extension/themes/resiris-nord-theme.json",
    }
    assert expected <= names
