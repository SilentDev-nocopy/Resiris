"use strict";

const vscode = require("vscode");

let MODULES = [];
try {
  MODULES = require("./modules.json").modules || [];
} catch (error) {
  MODULES = [];
}

const MISSING_INCLUDE = "resiris.missing-include";
// Only trigger the suggestion widget in unambiguous contexts. Triggering on
// every letter keeps the widget open through ordinary words (e.g. while typing
// "v x ModuleObject = "); once the word is empty every snippet matches, so a
// stray Tab expands a whole snippet. "." / "[" open module members, ">" opens
// the module list right after "<include>". Word completion stays available on
// demand via Ctrl+Space.
const TRIGGER_CHARS = [".", "[", ">"];

function activate(context) {
  // ---------- diagnostics: use of a module that is not included ----------
  const collection = vscode.languages.createDiagnosticCollection("resiris");
  context.subscriptions.push(collection);

  function refreshDiagnostics(document) {
    if (!document || document.languageId !== "resiris") return;

    const included = getIncludedModules(document.getText());
    const problems = findMissingModuleUses(document.getText(), included);

    const diagnostics = problems.map((problem) => {
      const range = new vscode.Range(
        document.positionAt(problem.offset),
        document.positionAt(problem.offset + problem.length)
      );
      const diagnostic = new vscode.Diagnostic(
        range,
        `The '${problem.name}' module is used but not included. Add <include> ${problem.name} at the top of the file.`,
        vscode.DiagnosticSeverity.Warning
      );
      diagnostic.code = MISSING_INCLUDE;
      diagnostic.source = "Resiris Language Support";
      return diagnostic;
    });

    collection.set(document.uri, diagnostics);
  }

  context.subscriptions.push(
    vscode.workspace.onDidChangeTextDocument((event) =>
      refreshDiagnostics(event.document)
    ),
    vscode.workspace.onDidOpenTextDocument(refreshDiagnostics),
    vscode.workspace.onDidCloseTextDocument((document) =>
      collection.delete(document.uri)
    )
  );
  if (vscode.window.activeTextEditor) {
    refreshDiagnostics(vscode.window.activeTextEditor.document);
  }

  // ---------- code actions: insert the missing include ----------
  const codeActionProvider = vscode.languages.registerCodeActionsProvider(
    { language: "resiris" },
    {
      provideCodeActions(document, range, context) {
        const actions = [];
        const included = getIncludedModules(document.getText());
        const queued = new Set();

        for (const diagnostic of context.diagnostics || []) {
          if (diagnostic.code !== MISSING_INCLUDE) continue;
          const moduleName =
            (diagnostic.moduleName || "").trim() ||
            moduleNameFromMessage(diagnostic.message);
          if (!moduleName || included.has(moduleName)) continue;
          if (queued.has(moduleName)) continue;
          queued.add(moduleName);

          const action = new vscode.CodeAction(
            `Add <include> ${moduleName}`,
            vscode.CodeActionKind.QuickFix
          );
          action.edit = insertIncludesEdit(document, [moduleName]);
          action.diagnostics = [diagnostic];
          action.isPreferred = true;
          actions.push(action);
        }
        return actions;
      },
    }
  );
  context.subscriptions.push(codeActionProvider);

  // ---------- completions ----------
  const provider = vscode.languages.registerCompletionItemProvider(
    { language: "resiris" },
    {
      provideCompletionItems(document, position) {
        const lineText = document.lineAt(position).text;
        const before = lineText.slice(0, position.character);

        // Never complete inside a comment or an unterminated string literal.
        const state = scanLine(before);
        if (state.inComment || state.inString) return [];

        // Typing after an <include>: offer the not-yet-included modules.
        const includeMatch = /<include>\s*([A-Za-z_]*)$/.exec(before);
        if (includeMatch) {
          const prefix = includeMatch[1];
          return preselect(
            moduleNameItems(
              document,
              prefix,
              replaceRange(position, prefix.length),
              true
            )
          );
        }

        // Access contexts (Module.fn, Module[CONST, obj.method chains).
        const access = accessCompletions(document, before, position);
        if (access !== undefined) return preselect(access);

        const wordMatch = /([A-Za-z_][A-Za-z0-9_]*)$/.exec(before);
        const word = wordMatch ? wordMatch[1] : "";
        const statementStart = isStatementStart(before, word);

        if (word !== "") {
          const pool = statementStart ? statementItems(document) : expressionItems(document);
          return filterItems(pool, word);
        }

        if (state.parenDepth > 0) return [];
        if (/=\s*$/.test(before)) {
          return expressionItems(document);
        }
        if (statementStart) return statementItems(document);
        return [];
      },
    },
    ...TRIGGER_CHARS
  );
  context.subscriptions.push(provider);
}

function scanLine(text) {
  let inString = false;
  let quote = "";
  let escaped = false;
  let inComment = false;
  let parenDepth = 0;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (inComment) break;
    if (inString) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (ch === "\\") {
        escaped = true;
        continue;
      }
      if (ch === quote) inString = false;
      continue;
    }
    if (ch === '"' || ch === "'") {
      inString = true;
      quote = ch;
      continue;
    }
    if (ch === "#" && text[i + 1] === "#") {
      inComment = true;
      continue;
    }
    if (ch === "(") parenDepth++;
    else if (ch === ")") parenDepth = Math.max(0, parenDepth - 1);
  }
  return { inComment, inString, parenDepth };
}

function isStatementStart(before, word) {
  const head = word ? before.slice(0, before.length - word.length) : before;
  const trimmed = head.trim();
  if (trimmed === "") return true;
  return trimmed.endsWith(":");
}

function replaceRange(position, length) {
  const start = Math.max(0, position.character - length);
  return new vscode.Range(new vscode.Position(position.line, start), position);
}

function accessCompletions(document, before, position) {
  const moduleConst = /\b([A-Z][A-Za-z0-9_]*)\s*\[\s*([A-Za-z_]*)$/.exec(before);
  if (moduleConst) {
    const module = MODULES.find((m) => m.name === moduleConst[1]);
    if (!module) return [];
    const tail = moduleConst[2];
    const range = replaceRange(position, tail.length);
    return module.variables
      .filter((v) => v.name.toLowerCase().startsWith(tail.toLowerCase()))
      .map((v) => constantItem(module, v, range));
  }

  // Any other bracket index (e.g. "arr[", "arr[i") has no known constants.
  if (/\b([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*[A-Za-z_]*$/.test(before)) return [];

  // Module member access: Module.function
  const moduleMember = /\b([A-Z][A-Za-z0-9_]*)\s*\.\s*([A-Za-z_]*)$/.exec(before);
  if (moduleMember) {
    const module = MODULES.find((m) => m.name === moduleMember[1]);
    if (!module) return [];
    const tail = moduleMember[2];
    const range = replaceRange(position, tail.length);
    return module.functions
      .filter((f) => f.toLowerCase().startsWith(tail.toLowerCase()))
      .map((f) => functionItem(module, f, range));
  }

  // Object member access: a ModuleObject variable or a chain of method calls
  // rooted in one, e.g. "timer.set_timer(0.3).on_timeout".
  const chain = /\b([a-z_][A-Za-z0-9_]*)(?:\.[A-Za-z_][A-Za-z0-9_]*\([^)]*\))*\s*\.\s*([A-Za-z_]*)$/.exec(
    before
  );
  if (chain) {
    const module = resolveObjectModule(document, chain[1]);
    if (!module) return [];
    const tail = chain[2];
    const range = replaceRange(position, tail.length);
    return module.functions
      .filter((f) => f.toLowerCase().startsWith(tail.toLowerCase()))
      .map((f) => functionItem(module, f, range));
  }

  // Access punctuation typed after a non-module owner (e.g. "30.", "arr[",
  // "timer." when "timer" is not a known module object): nothing to offer.
  if (/[.\[]\s*$/.test(before)) return [];
  return undefined;
}

function resolveObjectModule(document, objectName) {
  const re = new RegExp(
    "^[ \\t]*v\\s+" +
      objectName +
      "\\s+(?:ModuleObject|UnknownObject|FunctionalObject)\\s*=\\s*([A-Z][A-Za-z0-9_]*)\\s*(?:\\.|\\[)",
    "gm"
  );
  const match = re.exec(document.getText());
  if (!match) return undefined;
  return MODULES.find((m) => m.name === match[1]);
}

function moduleNameItems(document, prefix, range, excludeIncluded) {
  const included = getIncludedModules(document.getText());
  return MODULES.filter(
    (m) =>
      (!excludeIncluded || !included.has(m.name)) &&
      m.name.toLowerCase().startsWith(prefix.toLowerCase())
  ).map((m) => {
    const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Module);
    item.detail = "Resiris module";
    item.documentation = m.documentation || "";
    item.insertText = m.name;
    item.sortText = "\u0000" + m.name;
    if (range) item.range = range;
    return item;
  });
}

function functionItem(module, f, range) {
  const item = new vscode.CompletionItem(f, vscode.CompletionItemKind.Function);
  item.detail = `${module.name}.${f}()`;
  item.documentation = new vscode.MarkdownString(
    `Resiris module function \`${module.name}.${f}(...)\`\n\n*Requires:* \`<include> ${module.name}\``
  );
  item.insertText = new vscode.SnippetString(`${f}(\${1})`);
  item.sortText = "\u0000" + f;
  if (range) item.range = range;
  return item;
}

function constantItem(module, v, range) {
  const item = new vscode.CompletionItem(v.name, vscode.CompletionItemKind.Constant);
  item.detail = `${module.name}[${v.name}] : ${v.type}`;
  item.documentation = new vscode.MarkdownString(
    `Module constant \`${module.name}[${v.name}]\` of type \`${v.type}\`\n\n*Requires:* \`<include> ${module.name}\``
  );
  item.insertText = v.name;
  item.sortText = "\u0000" + v.name;
  if (range) item.range = range;
  return item;
}

function statementItems(document) {
  return [
    buildStartItem(),
    buildProcessItem(document),
    buildIncludeItem(),
    buildKeyword("v", "Variable declaration", "v ${1:name} ${2:int} = ${3:0}"),
    buildKeyword("c", "Constant declaration", "c ${1:NAME} ${2:int} = ${3:0}"),
    buildKeyword("fn", "Function definition", "fn ${1:name}(${2}):\n\t$0"),
    buildKeyword("if", "Conditional branch", "if ${1:condition}:\n\t$0"),
    buildKeyword("elif", "Else-if branch", "elif ${1:condition}:\n\t$0"),
    buildKeyword("else", "Else branch", "else:\n\t$0"),
    buildKeyword("mat", "Matrix operation", "mat ${1:...}"),
    buildKeyword("return", "Return a value", "return $0"),
    buildKeyword("pass", "Do nothing", "pass"),
  ];
}

const RESIRIS_TYPES = [
  "int",
  "float",
  "string",
  "bool",
  "ModuleObject",
  "FunctionalObject",
  "UnknownObject",
];

function expressionItems(document) {
  const items = moduleNameItems(document, "", null, false);
  for (const typeName of RESIRIS_TYPES) {
    items.push(typeItem(typeName));
  }
  items.push(booleanItem("true"));
  items.push(booleanItem("false"));
  return items;
}

function preselect(items) {
  if (items.length > 0) items[0].preselect = true;
  return items;
}

function filterItems(items, word) {
  const needle = word.toLowerCase();
  return items.filter(
    (item) =>
      item.label.toLowerCase().startsWith(needle) ||
      (item.filterText || "").toLowerCase().startsWith(needle)
  );
}

function buildKeyword(label, detail, body) {
  const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.Keyword);
  item.detail = detail;
  item.insertText = new vscode.SnippetString(body);
  item.filterText = label;
  item.sortText = "5" + label;
  return item;
}

function typeItem(typeName) {
  const item = new vscode.CompletionItem(typeName, vscode.CompletionItemKind.Type);
  item.detail = "Resiris data type";
  item.filterText = typeName;
  item.sortText = "6" + typeName;
  return item;
}

function booleanItem(label) {
  const item = new vscode.CompletionItem(label, vscode.CompletionItemKind.Keyword);
  item.detail = "Boolean literal";
  item.insertText = label;
  item.filterText = label;
  item.sortText = "7" + label;
  return item;
}

function getIncludedModules(text) {
  const included = new Set();
  const re = /^[ \t]*<include>[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*(?:##.*)?$/gm;
  let match;
  while ((match = re.exec(text)) !== null) {
    included.add(match[1]);
  }
  return included;
}

function forEachLine(text, callback) {
  let start = 0;
  let index = text.indexOf("\n");
  while (index !== -1) {
    if (text[index - 1] === "\r") {
      callback(start, text.slice(start, index - 1));
    } else {
      callback(start, text.slice(start, index));
    }
    start = index + 1;
    index = text.indexOf("\n", start);
  }
  callback(start, text.slice(start));
}

function findMissingModuleUses(text, included) {
  const missing = new Map();
  const access = /(?<!\[)(?<!\.)\b([A-Z][A-Za-z0-9_]*)\s*(?:\.|\[)/g;

  forEachLine(text, (start, lineText) => {
    const line = lineText.replace(/##.*$/, "");
    if (line.trim() === "") return;

    access.lastIndex = 0;
    let match;
    while ((match = access.exec(line)) !== null) {
      const name = match[1];
      if (!included.has(name) && !missing.has(name)) {
        missing.set(name, {
          name,
          offset: start + match.index,
          length: name.length,
        });
      }
    }
  });

  for (const module of MODULES) {
    const name = module.name;
    if (included.has(name) || missing.has(name)) continue;
    const word = new RegExp("\\b" + name + "\\b");
    forEachLine(text, (start, lineText) => {
      if (missing.has(name)) return;
      const line = lineText.replace(/##.*$/, "");
      const trimmed = line.trim();
      if (trimmed === "" || /^<include>\s+/.test(trimmed)) return;
      const at = line.search(word);
      if (at !== -1) {
        missing.set(name, { name, offset: start + at, length: name.length });
      }
    });
  }

  return [...missing.values()];
}

function moduleNameFromMessage(message) {
  const match = /The '([^']+)' module/.exec(message || "");
  return match ? match[1] : "";
}

function insertIncludesEdit(document, moduleNames) {
  const normalize = (value) => value.replace(/\r\n/g, "\n");
  const lines = normalize(document.getText()).split("\n");
  const eol = document.eol === vscode.EndOfLine.CRLF ? "\r\n" : "\n";

  let insertionLine = 0;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (line === "" || line.startsWith("##")) continue;
    if (/^<include>\s+[A-Za-z_][A-Za-z0-9_]*\s*(?:##.*)?$/.test(line)) {
      insertionLine = i + 1;
      continue;
    }
    break;
  }

  const block =
    [...moduleNames]
      .sort()
      .map((name) => `<include> ${name}`)
      .join(eol) + eol;

  let inserted = block;
  if (insertionLine >= document.lineCount) {
    const raw = document.getText();
    if (raw.length > 0 && !/[\r\n]$/.test(raw)) {
      inserted = eol + block;
    }
  }

  const edit = new vscode.WorkspaceEdit();
  edit.insert(document.uri, new vscode.Position(insertionLine, 0), inserted);
  return edit;
}

function buildStartItem() {
  const start = new vscode.CompletionItem("start", vscode.CompletionItemKind.Snippet);
  start.detail = "Resiris start";
  start.documentation = "Resiris lifecycle entry point";
  start.insertText = new vscode.SnippetString("START():\n\t$0");
  start.filterText = "start";
  start.sortText = "1start";
  return start;
}

function buildProcessItem(document) {
  const process = new vscode.CompletionItem("process", vscode.CompletionItemKind.Snippet);
  process.detail = "Resiris process";
  process.documentation = "Resiris repeated lifecycle function";
  if (hasFpsConstant(document)) {
    process.insertText = new vscode.SnippetString("PROCESS(FPS):\n\t$0");
  } else {
    process.insertText = new vscode.SnippetString(
      "## FPS: how many times PROCESS runs per second\n" +
        "c FPS float = 30.0\n" +
        "\n" +
        "PROCESS(FPS):\n" +
        "\t$0"
    );
  }
  process.filterText = "process";
  process.sortText = "2process";
  return process;
}

function buildIncludeItem() {
  const include = new vscode.CompletionItem("include", vscode.CompletionItemKind.Snippet);
  include.detail = "Resiris module include";
  include.documentation = "Include a module so its functions and constants can be used";
  include.insertText = new vscode.SnippetString("<include> ${1:ModuleName}");
  include.filterText = "include";
  include.sortText = "3include";
  return include;
}

function hasFpsConstant(document) {
  return /^\s*c\s+FPS\s+float\b/m.test(document.getText());
}

function deactivate() {}

module.exports = { activate, deactivate };