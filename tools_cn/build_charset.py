#!/usr/bin/env python3
"""Build the character set needed by the Chinese UI, then subset msyh.ttc."""
import json
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

dict_data = json.loads(Path("tools_cn/dict.json").read_text(encoding="utf-8"))

chars = set()
for v in dict_data.values():
    for ch in v:
        if ord(ch) >= 0x20:
            chars.add(ch)

# ASCII printable (for safety when Hack is not used / mixed text)
for cp in range(0x20, 0x7F):
    chars.add(chr(cp))

# Common CJK punctuation & symbols
extra = "，。！？：；（）【】《》〈〉「」『』…—–·、％℃×÷＝＋－＊／＃＠＆《》"
for ch in extra:
    chars.add(ch)

chars = sorted(chars)
text = "".join(chars)
Path("tools_cn/cjk_chars.txt").write_text(text, encoding="utf-8")
print(f"total unique chars: {len(chars)}")

# Show the non-ASCII chars for sanity
non_ascii = [c for c in chars if ord(c) > 0x7F]
print("sample non-ascii:", "".join(non_ascii[:80]))
print(f"non-ascii count: {len(non_ascii)}")
