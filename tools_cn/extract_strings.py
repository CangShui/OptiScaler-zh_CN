#!/usr/bin/env python3
"""Extract C/C++ string literals from OptiScaler menu files with context."""
import re
import json
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
SRC = BASE / "OptiScaler" / "menu"
OUT = BASE / "tools_cn" / "strings_extracted.json"

# Match string literals: "..." with escapes, and adjacent concatenation
STR_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')

def decode_c(s: str) -> str:
    """Properly decode C string literal escapes (content only)."""
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

def extract(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.split("\n")
    results = []  # (value, line_no, context_line, context_prev)
    for i, line in enumerate(lines, 1):
        # skip comments-only handling is tricky; capture raw matches
        for m in STR_RE.finditer(line):
            raw = m.group(1)
            value = decode_c(raw)
            if len(value) < 2:
                continue
            # skip strings that are obviously not UI (long paths, format specs only)
            ctx = line.strip()
            results.append({"value": value, "raw": raw, "line": i, "ctx": ctx[:160]})
    return results

all_results = {}
for f in sorted(SRC.glob("*.cpp")):
    for r in extract(f):
        key = r["value"]
        if key not in all_results:
            all_results[key] = {"value": key, "count": 0, "files": []}
        all_results[key]["count"] += 1
        entry = all_results[key]
        if len(entry["files"]) < 6:
            entry["files"].append({"file": f.name, "line": r["line"], "ctx": r["ctx"]})

items = sorted(all_results.values(), key=lambda x: -x["count"])
print(f"Total unique strings: {len(items)}")
OUT.write_text(json.dumps(items, ensure_ascii=False, indent=1), encoding="utf-8")
print(f"Written to {OUT}")

# Print strings with their context for the first pass review
for it in items:
    f0 = it["files"][0]
    print(f"{it['count']:4d} | {it['value']!r}")
    print(f"      {f0['file']}:{f0['line']}: {f0['ctx']}")
