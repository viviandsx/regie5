#!/usr/bin/env python3
# Regie5 — prints every interface string, one column per language, as a
# Markdown table (for proofreading and translating). Read from src/core/lang/.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/list-strings.py > build/strings.md          every language
#   scripts/list-strings.py de > build/strings-de.md    English and German only
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LANG = os.path.join(ROOT, "src", "core", "lang")
HEADER = os.path.join(ROOT, "src", "core", "i18n.h")
entry = re.compile(r'\{\s*(\w+)\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}', re.S)


def joined(literals):
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', literals))


h = open(HEADER, encoding="utf-8").read()
keys = re.findall(r"^\s*(\w+),", h[h.index("enum S {") + 8:h.index("    Count\n")], re.M)
langs = {}
for name in sorted(os.listdir(LANG)):
    if not name.endswith(".cpp") or name == "languages.cpp":
        continue
    text = open(os.path.join(LANG, name), encoding="utf-8").read()
    code = re.search(r'extern const Language \w+ = \{\s*"([^"]+)"', text).group(1)
    langs[code] = {k: joined(v) for k, v in entry.findall(text[text.index("kTexts[] = {"):])}
wanted = ["en"] + [c for c in sys.argv[1:] if c != "en"] if len(sys.argv) > 1 else \
    ["en"] + sorted(c for c in langs if c != "en")
missing = [c for c in wanted if c not in langs]
if missing:
    sys.exit("no such language in src/core/lang/: " + ", ".join(missing))
print("| Key | " + " | ".join(wanted) + " |")
print("|---|" + "---|" * len(wanted))
for k in keys:
    cell = lambda c: langs[c].get(k, "*(missing)*").replace("|", "\\|")
    print("| %s | %s |" % (k, " | ".join(cell(c) for c in wanted)))
for c in wanted:
    print("%s: %d of %d strings" % (c, sum(k in langs[c] for k in keys), len(keys)), file=sys.stderr)
