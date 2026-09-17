#!/usr/bin/env python3
import json
import os
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "build"
STAGING = OUT / "extension"
VERSION = "0.5.0"
EXTENSION_ID = "resiris-language-support"

cands = [
    Path("/usr/share/code/resources/app/extensions/theme-seti/icons"),
    Path("/usr/lib/code/resources/app/extensions/theme-seti/icons"),
    Path("/opt/visual-studio-code/resources/app/extensions/theme-seti/icons"),
    Path("/snap/code/current/usr/share/code/resources/app/extensions/theme-seti/icons"),
]
for cmd in ("code", "code-insiders", "codium"):
    exe = shutil.which(cmd)
    if exe:
        real = Path(os.path.realpath(exe))
        for parent in [real.parent, *real.parents]:
            cands += [
                parent / "resources/app/extensions/theme-seti/icons",
                parent / "share/code/resources/app/extensions/theme-seti/icons",
            ]

seti = None
for p in cands:
    try:
        p = p.resolve()
    except OSError:
        continue
    if (p / "vs-seti-icon-theme.json").is_file() and (p / "seti.woff").is_file():
        seti = p
        break

ASSET_THEME = ROOT / "assets/resiris-seti-icon-theme.json"
ASSET_FONT = ROOT / "assets/seti.woff"

if seti is None and (not ASSET_THEME.is_file() or not ASSET_FONT.is_file()):
    print("ERROR: VS Code Seti theme not found and bundled Seti assets are missing.", file=sys.stderr)
    sys.exit(2)

if STAGING.exists():
    shutil.rmtree(STAGING)
(STAGING / "icons").mkdir(parents=True)
(STAGING / "syntaxes").mkdir(parents=True)
(STAGING / "themes").mkdir(parents=True)
OUT.mkdir(exist_ok=True)

for filename in ("package.json", "language-configuration.json"):
    shutil.copy2(ROOT / filename, STAGING / filename)
shutil.copy2(ROOT / "icons/resy.png", STAGING / "icons/resy.png")
shutil.copy2(ROOT / "syntaxes/resiris.tmLanguage.json", STAGING / "syntaxes/resiris.tmLanguage.json")
for theme_file in sorted((ROOT / "themes").glob("*.json")):
    shutil.copy2(theme_file, STAGING / "themes" / theme_file.name)

if seti is not None:
    shutil.copy2(seti / "seti.woff", STAGING / "icons/seti.woff")
    theme = json.loads((seti / "vs-seti-icon-theme.json").read_text(encoding="utf-8"))
    theme.setdefault("iconDefinitions", {})["_resy"] = {"iconPath": "./resy.png"}
    theme.setdefault("fileExtensions", {})["resy"] = "_resy"
    for mode in ("light", "highContrast"):
        theme.setdefault(mode, {}).setdefault("fileExtensions", {})["resy"] = "_resy"
else:
    shutil.copy2(ASSET_FONT, STAGING / "icons/seti.woff")
    theme = json.loads(ASSET_THEME.read_text(encoding="utf-8"))

(STAGING / "icons/resiris-seti-icon-theme.json").write_text(
    json.dumps(theme, indent=2) + "\n", encoding="utf-8"
)

manifest = "\n".join([
    '<?xml version="1.0" encoding="utf-8"?>',
    '<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">',
    '  <Metadata>',
    f'    <Identity Id="{EXTENSION_ID}" Version="{VERSION}" Language="en" Publisher="resiris" />',
    '    <DisplayName>Resiris Language Support</DisplayName>',
    '    <Description xml:space="preserve">Language support and Seti file icons for the Resiris programming language.</Description>',
    '    <Categories>Programming Languages;Themes</Categories>',
    '  </Metadata>',
    '  <Installation><InstallationTarget Id="Microsoft.VisualStudio.Code" Version="[1.80.0,2.0.0)" /></Installation>',
    '  <Dependencies />',
    '  <Assets>',
    '    <Asset Type="Microsoft.VisualStudio.Services.VSIXManifest" Path="extension.vsixmanifest" />',
    '    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" />',
    '  </Assets>',
    '</PackageManifest>',
])

vsix = OUT / f"{EXTENSION_ID}-{VERSION}.vsix"
with zipfile.ZipFile(vsix, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("extension.vsixmanifest", manifest)
    for p in STAGING.rglob("*"):
        if p.is_file():
            z.write(p, "extension/" + str(p.relative_to(STAGING)))

print(vsix)