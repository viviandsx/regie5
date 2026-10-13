#!/usr/bin/env python3
# Regie5 — prints every interface string, French and English side by side, as
# a Markdown table (for proofreading). Read from src/core/i18n.cpp.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   scripts/list-strings.py > build/strings.md
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "core", "i18n.cpp")

text = open(SRC, encoding="utf-8").read()
table = text[text.index("kTable[Count] = {"):]
entry = re.compile(r'/\*\s*(\w+)\s*\*/\s*\{\s*((?:"(?:[^"\\]|\\.)*"\s*)+),\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}', re.S)


def joined(literals):
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', literals))


rows = entry.findall(table)
if not rows:
    sys.exit("no strings found in " + SRC)
print("| Clé | Français | English |")
print("|---|---|---|")
for key, fr, en in rows:
    cell = lambda s: joined(s).replace("|", "\\|")
    print(f"| {key} | {cell(fr)} | {cell(en)} |")
print(f"\n{len(rows)} chaînes.", file=sys.stderr)
