#!/usr/bin/env python3
"""Apply Chinese translation dictionary to OptiScaler menu source files.

- Translates string literals that are NOT inside comments.
- Preserves comments, line endings (CRLF/LF) and BOM of original files.
- Idempotent: already-translated strings are left untouched.
"""
import json
import re
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
MENU = BASE / "OptiScaler" / "menu"
DICT = json.loads(Path(BASE / "tools_cn" / "dict.json").read_text(encoding="utf-8"))
assert all(v for v in DICT.values()), "empty translation value"


def decode_c(s: str) -> str:
    out = []
    i = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        i += 1
        if i >= n:
            out.append("\\")
            break
        e = s[i]
        i += 1
        if e == "n":
            out.append("\n")
        elif e == "r":
            out.append("\r")
        elif e == "t":
            out.append("\t")
        elif e == "a":
            out.append("\a")
        elif e == "b":
            out.append("\b")
        elif e == "f":
            out.append("\f")
        elif e == "v":
            out.append("\v")
        elif e == "0":
            out.append("\0")
        elif e in "\\\"'?":
            out.append(e)
        elif e == "x":
            h = s[i:i + 2]
            out.append(chr(int(h, 16)) if h else "x")
            i += len(h)
        elif e == "u":
            h = s[i:i + 4]
            out.append(chr(int(h, 16)) if len(h) == 4 else "u")
            i += len(h)
        elif e == "U":
            h = s[i:i + 8]
            out.append(chr(int(h, 16)) if len(h) == 8 else "U")
            i += len(h)
        else:
            out.append(e)
    return "".join(out)


def encode_c(s: str) -> str:
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        else:
            out.append(ch)
    return "".join(out)


def process_line(line: str, in_block: bool):
    """Return (new_line, new_in_block). Preserves comments and non-dict strings."""
    out = []
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if in_block:
            end = line.find("*/", i)
            if end == -1:
                out.append(line[i:])
                return "".join(out), True
            out.append(line[i:end + 2])
            i = end + 2
            in_block = False
            continue
        if line.startswith("//", i):
            out.append(line[i:])
            return "".join(out), False
        if c == "/" and i + 1 < n and line[i + 1] == "*":
            end = line.find("*/", i + 2)
            if end == -1:
                out.append(line[i:])
                return "".join(out), True
            out.append(line[i:end + 2])
            i = end + 2
            continue
        if c == '"':
            j = i + 1
            while j < n:
                if line[j] == "\\":
                    j += 2
                    continue
                if line[j] == '"':
                    j += 1
                    break
                j += 1
            raw = line[i + 1:j - 1]
            value = decode_c(raw)
            if value in DICT:
                out.append('"' + encode_c(DICT[value]) + '"')
            else:
                out.append(line[i:j])
            i = j
            continue
        out.append(c)
        i += 1
    return "".join(out), in_block


def replace_in_file(path: Path) -> int:
    data = path.read_bytes()
    had_bom = data.startswith(b"\xef\xbb\xbf")
    crlf = b"\r\n" in data
    text = data.decode("utf-8-sig")
    lines = text.splitlines()
    replaced = 0
    in_block = False
    for li, line in enumerate(lines):
        new_line, in_block = process_line(line, in_block)
        lines[li] = new_line
    new_text = "\n".join(lines)
    if text.endswith("\n") and not new_text.endswith("\n"):
        new_text += "\n"
    enc = "utf-8-sig" if had_bom else "utf-8"
    path.write_text(new_text, encoding=enc, newline="\r\n" if crlf else "\n")
    return count_replaced(text, new_text)


def count_replaced(old: str, new: str) -> int:
    """Count dict-key literal occurrences removed from old text."""
    cnt = 0
    for k in DICT:
        kenc = encode_c(k)
        delta = old.count(f'"{kenc}"') - new.count(f'"{kenc}"')
        if delta > 0:
            cnt += delta
    return cnt


total = 0
for f in sorted(MENU.glob("*.cpp")):
    n = replace_in_file(f)
    total += n
    print(f"{f.name}: {n} keys translated")

# report keys still present as English literals (excluding identity translations)
text_all = ""
for f in MENU.glob("*.cpp"):
    text_all += f.read_text(encoding="utf-8-sig")

missing = []
for k in DICT:
    if DICT[k] == k:
        continue  # identity translation - nothing to detect
    kenc = encode_c(k)
    if f'"{kenc}"' in text_all:
        missing.append(k)
print(f"\nTotal keys translated: {total}")
print(f"Keys still present in source (untranslated): {len(missing)}")
for k in missing:
    print("  MISS:", repr(k))
