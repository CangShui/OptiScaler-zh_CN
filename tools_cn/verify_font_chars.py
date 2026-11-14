#!/usr/bin/env python3
"""Verify every Chinese char used in menu sources exists in the CJK subset font."""
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
from fontTools.ttLib import TTFont

font = TTFont("tools_cn/msyh_cjk.ttf")
cmap = font.getBestCmap()

missing = set()
used = set()
for f in Path("OptiScaler/menu").glob("*.cpp"):
    text = f.read_text(encoding="utf-8-sig")
    for ch in text:
        if "\u4e00" <= ch <= "\u9fff" or "\u3000" <= ch <= "\u303f" or "\uff00" <= ch <= "\uffef" or ch in "，。！？：；（）《》〈〉「」『』…—–·、％℃×÷＝＋－＊／＃＠＆":
            used.add(ch)
            if ord(ch) not in cmap:
                missing.add(ch)

print("unique CJK/punct chars used in source:", len(used))
if missing:
    print("MISSING from font:", "".join(sorted(missing)))
else:
    print("ALL PRESENT - OK")
