#!/usr/bin/env python3
"""Merge the new UI translations into dict_v11.json."""
import json
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")

BASE = Path(__file__).resolve().parent
old = json.loads((BASE / "dict_v10.json").read_text(encoding="utf-8"))

from dict_v11_extra import EXTRA

existing = BASE / "dict_v11.json"
merged = json.loads(existing.read_text(encoding="utf-8")) if existing.exists() else dict(old)
changed = []
for key, value in EXTRA.items():
    if key in merged and merged[key] != value:
        changed.append((key, merged[key], value))
    merged[key] = value

if changed:
    print("Overriding existing translations:")
    for key, before, after in changed:
        print(f"  {key!r}: {before!r} -> {after!r}")

missing = [k for k, v in merged.items() if not v]
assert not missing, f"empty translations: {missing}"

(BASE / "dict_v11.json").write_text(
    json.dumps(merged, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
)
print(f"dict_v11.json written: {len(merged)} keys ({len(EXTRA)} new/updated)")
