#!/usr/bin/env python3
"""Filter extracted strings to UI-related ones and dump for review."""
import json
import re
from pathlib import Path

BASE = Path(__file__).resolve().parent
items = json.loads((BASE / "strings_extracted.json").read_text(encoding="utf-8"))

UI_CTX = re.compile(
    r"ImGui::(Text|Button|Checkbox|Slider|Combo|BeginCombo|Selectable|Input|ColorEdit|ColorPicker|RadioButton|BulletText|LabelText|CollapsingHeader|SeparatorText|InputText|DragInt|DragFloat)|"
    r"ShowHelpMarker|ShowTooltip|PopulateCombo|AddDLSS|AddResourceBarrier|ScopedCollapsingHeader|"
    r"SeparatorTextEx|SetTooltip|BeginMenu|MenuItem|TreeNode|OpenWiki|BeginChild"
)

def is_ui(item):
    return any(UI_CTX.search(f["ctx"]) for f in item["files"])

ui_items = [it for it in items if is_ui(it)]
non_ui = [it for it in items if not is_ui(it)]

lines = []
lines.append(f"=== UI STRINGS ({len(ui_items)}) ===")
for it in sorted(ui_items, key=lambda x: x["value"]):
    f0 = it["files"][0]
    lines.append(f'{it["count"]:4d} | {it["value"]!r}')
    lines.append(f"      {f0['file']}:{f0['line']}: {f0['ctx']}")

lines.append("")
lines.append(f"=== NON-UI STRINGS ({len(non_ui)}) ===")
for it in sorted(non_ui, key=lambda x: x["value"]):
    f0 = it["files"][0]
    lines.append(f'{it["count"]:4d} | {it["value"]!r}')
    lines.append(f"      {f0['file']}:{f0['line']}: {f0['ctx']}")

(BASE / "strings_review.txt").write_text("\n".join(lines), encoding="utf-8")
print(f"UI: {len(ui_items)}, NON-UI: {len(non_ui)} -> strings_review.txt")
