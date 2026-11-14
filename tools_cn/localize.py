#!/usr/bin/env python3
"""OptiScaler 汉化维护工具。

常用命令：
    python tools_cn/localize.py report
    python tools_cn/localize.py apply
    python tools_cn/localize.py validate
    python tools_cn/localize.py font
    python tools_cn/localize.py upstream

report 会列出当前源码中新出现、尚未进入字典的界面字符串，并给出相近译文。
把译文按“英文<TAB>中文”写入 TSV 后，用 import 合并进字典。
"""
from __future__ import annotations

import argparse
import difflib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools_cn"
MENU = ROOT / "OptiScaler" / "menu"
DICT_PATH = TOOLS / "dict_v11.json"

STR_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
FORMAT_RE = re.compile(r"%(?:\d+\$)?[-+ #0]*\d*(?:\.\d+)?[diuoxXfFeEgGaAcspn%]")

UI_HINT = re.compile(
    r"ImGui::(Text|TextWrapped|TextDisabled|TextColored|TextUnformatted|Button|SmallButton|"
    r"Checkbox|Slider|Combo|BeginCombo|Selectable|Input|ColorEdit|ColorPicker|RadioButton|"
    r"BulletText|LabelText|CollapsingHeader|SeparatorText|InputText|DragInt|DragFloat)|"
    r"ShowTooltip|SetTooltip|ShowHelpMarker|SectionTitle|SeparatorWithHelpMarker|"
    r"ConfigCheckbox|optionCheckbox|PopulateCombo|StatusFlag|ScopedCard card|"
    r"pair\(|flag\(|row\(|colorEdit\(|scaleSlider\(|statusText\("
)

TECH_HINT = re.compile(
    r"LOG_|spdlog|printf|OutputDebugString|https?://|\.dll|\.ttf|\.otf|\.ini|\.reg|"
    r"HKLM|DWORD|std::filesystem|CreateFile|GetProcAddress|LoadLibrary|"
    r"#include|SetWindowText|ShellExecute"
)

PRODUCT_ALLOW = {
    "AUTO", "COMMON", "DLSS", "DLSSD", "DLSSG", "FSR", "FSR2", "FSR3", "FSR4",
    "XeSS", "XeFG", "XeLL", "FFX", "Vulkan", "DXGI", "DXVK", "VKD3D", "Linux",
    "OptiScaler", "OptiPatcher", "Nukems", "Enabler", "Combo", "Reflex",
    "LatencyFlex", "AntiLag", "RCAS", "DLAA", "HUDless", "HUDFix", "OptiFG",
}

TECH_VALUES = {
    "OM", "Ups", "DI", "Disp", "Opti", "FSR %s", "FSR %s##%d",
    "FFX 3.1", "FFX SR", "FFX Vk", "FFX Vk SR", "FFX Vk FG",
    "（DRS）",
}


def decode_c(raw: str) -> str:
    out: list[str] = []
    i = 0
    n = len(raw)
    simple = {
        "n": "\n", "r": "\r", "t": "\t", "a": "\a", "b": "\b",
        "f": "\f", "v": "\v", "0": "\0", "\\": "\\", '"': '"', "'": "'", "?": "?",
    }
    while i < n:
        c = raw[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        i += 1
        if i >= n:
            out.append("\\")
            break
        e = raw[i]
        i += 1
        if e in simple:
            out.append(simple[e])
        elif e == "x":
            h = raw[i:i + 2]
            out.append(chr(int(h, 16)) if h else "x")
            i += len(h)
        elif e == "u":
            h = raw[i:i + 4]
            out.append(chr(int(h, 16)) if len(h) == 4 else "u")
            i += len(h)
        elif e == "U":
            h = raw[i:i + 8]
            out.append(chr(int(h, 16)) if len(h) == 8 else "U")
            i += len(h)
        else:
            out.append(e)
    return "".join(out)


def encode_tsv(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")


def decode_tsv(value: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(value):
        if value[i] == "\\" and i + 1 < len(value):
            nxt = value[i + 1]
            if nxt == "n":
                out.append("\n")
                i += 2
                continue
            if nxt == "t":
                out.append("\t")
                i += 2
                continue
            if nxt == "\\":
                out.append("\\")
                i += 2
                continue
        out.append(value[i])
        i += 1
    return "".join(out)


def iter_literals(text: str):
    """Yield (line, decoded value, context), skipping comments."""
    in_block = False
    for lineno, line in enumerate(text.splitlines(), 1):
        i = 0
        n = len(line)
        while i < n:
            if in_block:
                end = line.find("*/", i)
                if end < 0:
                    break
                i = end + 2
                in_block = False
                continue
            if line.startswith("//", i):
                break
            if line.startswith("/*", i):
                end = line.find("*/", i + 2)
                if end < 0:
                    in_block = True
                    break
                i = end + 2
                continue
            if line[i] == '"':
                j = i + 1
                while j < n:
                    if line[j] == "\\":
                        j += 2
                        continue
                    if line[j] == '"':
                        break
                    j += 1
                if j < n:
                    yield lineno, decode_c(line[i + 1:j]), line.strip()
                    i = j + 1
                    continue
            i += 1


def source_text(ref: str | None) -> tuple[str, str]:
    rel = "OptiScaler/menu/menu_common.cpp"
    if ref:
        proc = subprocess.run(
            ["git", "show", f"{ref}:{rel}"],
            cwd=ROOT, capture_output=True, check=True,
        )
        return proc.stdout.decode("utf-8-sig", errors="replace"), f"{ref}:{rel}"
    path = ROOT / rel
    return path.read_text(encoding="utf-8-sig"), str(path)


def is_probably_ui(value: str, context: str) -> bool:
    if UI_HINT.search(context):
        return True
    if "[] = {" in context and ("const char*" in context or "static const" in context):
        return True
    if context.startswith("return ") and "?" not in context and len(value) < 80:
        return True
    return False


def is_technical(value: str, context: str) -> bool:
    if not value or value in PRODUCT_ALLOW or value in TECH_VALUES:
        return True
    if value.startswith("##"):
        return True
    if re.fullmatch(r"[-%0-9.,: xX()/]+", value):
        return True
    if "%" in value and not re.search(r"[A-Za-z]{3,}", value):
        return True
    if TECH_HINT.search(context):
        return True
    if re.fullmatch(r"[A-Z0-9_]+", value) and len(value) > 2:
        return True
    if re.fullmatch(r"[a-z0-9_]+", value):
        return True
    return False


def collect_untranslated(text: str, dictionary: dict[str, str], include_all: bool):
    seen: dict[str, dict] = {}
    for lineno, value, context in iter_literals(text):
        if not re.search(r"[A-Za-z]{2}", value):
            continue
        if re.search(r"[\u4e00-\u9fff]", value):
            continue
        if value in dictionary:
            continue
        ui = is_probably_ui(value, context)
        if not ui and not include_all:
            continue
        if is_technical(value, context):
            continue
        item = seen.setdefault(value, {"line": lineno, "context": context, "count": 0, "ui": ui})
        item["count"] += 1
    return seen


def suggest(value: str, dictionary: dict[str, str], count: int = 2):
    matches = difflib.get_close_matches(value, dictionary.keys(), n=count, cutoff=0.72)
    return [dictionary[m] for m in matches]


def cmd_report(args) -> int:
    text, label = source_text(args.ref)
    dictionary = json.loads(DICT_PATH.read_text(encoding="utf-8"))
    items = collect_untranslated(text, dictionary, args.all)
    rows = sorted(items.items(), key=lambda kv: kv[1]["line"])
    ui_rows = [(k, v) for k, v in rows if v["ui"]]
    other_rows = [(k, v) for k, v in rows if not v["ui"]]

    if args.tsv:
        out = Path(args.tsv)
        with out.open("w", encoding="utf-8", newline="\n") as fh:
            fh.write("english\ttranslation\tcontext\n")
            for value, info in rows:
                suggestion = " / ".join(suggest(value, dictionary))
                fh.write(
                    f"{encode_tsv(value)}\t{encode_tsv(suggestion)}\t"
                    f"{encode_tsv(info['context'])}\n"
                )
        print(f"已写出待翻译 TSV：{out}")
        print("把第二列改成中文后运行：python tools_cn/localize.py import " + str(out))
        return 0

    lines = [
        "OptiScaler 汉化报告",
        f"来源：{label}",
        f"字典：{DICT_PATH.relative_to(ROOT)}（{len(dictionary)} 条）",
        f"UI 待翻译：{len(ui_rows)}",
        f"其他英文：{len(other_rows)}",
        "",
        "## UI 待翻译",
    ]
    if not ui_rows:
        lines.append("（无）")
    for value, info in ui_rows:
        suggestion = " / ".join(suggest(value, dictionary))
        lines.append(f"{info['line']:5d} | {value!r}")
        if suggestion:
            lines.append(f"      建议：{suggestion}")
        lines.append(f"      上下文：{info['context']}")

    if args.all and other_rows:
        lines.extend(["", "## 其他英文（可能为日志/技术文本）"])
        for value, info in other_rows:
            lines.append(f"{info['line']:5d} | {value!r}")

    output = "\n".join(lines) + "\n"
    if args.out:
        Path(args.out).write_text(output, encoding="utf-8")
        print(f"已写出报告：{args.out}")
    else:
        print(output)
    return 0


def check_value(key: str, value: str) -> str | None:
    if not value:
        return "译文为空"
    if FORMAT_RE.findall(key) != FORMAT_RE.findall(value):
        return f"格式符不一致：{FORMAT_RE.findall(key)} -> {FORMAT_RE.findall(value)}"
    if key.count("{") != value.count("{") or key.count("}") != value.count("}"):
        return "花括号数量不一致"
    return None


def cmd_import(args) -> int:
    dictionary = json.loads(DICT_PATH.read_text(encoding="utf-8"))
    added = updated = skipped = 0
    for raw in Path(args.tsv).read_text(encoding="utf-8-sig").splitlines():
        if not raw.strip() or raw.startswith("#"):
            continue
        parts = raw.split("\t")
        if len(parts) < 2:
            continue
        key = decode_tsv(parts[0])
        value = decode_tsv(parts[1]).strip()
        if not key or not value:
            continue
        if key == "english" and value == "translation":
            continue
        problem = check_value(key, value)
        if problem:
            print(f"跳过：{key!r}（{problem}）")
            skipped += 1
            continue
        if key in dictionary:
            if dictionary[key] != value:
                updated += 1
            dictionary[key] = value
        else:
            added += 1
            dictionary[key] = value
    DICT_PATH.write_text(
        json.dumps(dictionary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"已合并：新增 {added}，更新 {updated}，跳过 {skipped}，字典共 {len(dictionary)} 条")
    return 0


def cmd_apply(_args) -> int:
    code = subprocess.run(
        [sys.executable, str(TOOLS / "apply_dict_v11.py")], cwd=ROOT
    ).returncode
    if code != 0:
        return code
    ensure_code_patches()
    return 0


FONT_BLOCK = """    // [CN] Merge the embedded CJK subset font so the Chinese UI renders correctly.
    {
        ImFontAtlas* atlas = io.Fonts;

        // Ensure a base font exists even when the HQ font is disabled.
        if (atlas->Fonts.empty())
            io.FontDefault = atlas->AddFontDefault();

        ImFontConfig cjkConfig;
        cjkConfig.MergeMode = true;
        cjkConfig.FontNo = 0;
        cjkConfig.OversampleH = 1;
        cjkConfig.OversampleV = 1;

        static const ImWchar cjkGlyphRanges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
            0x2000, 0x2BFF, // Punctuation, symbols, arrows, math
            0x3000, 0x303F, // CJK Symbols and Punctuation
            0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
            0x4E00, 0x9FAF, // CJK Unified Ideographs
            0,
        };

        atlas->AddFontFromMemoryCompressedTTF(cjk_compressed_data, cjk_compressed_size, fontSize, &cjkConfig,
                                              cjkGlyphRanges);
    }

"""


def ensure_code_patches() -> None:
    """保证上游新源码里仍有中文字体引入、字体合并和 /utf-8。"""
    menu_path = MENU / "menu_common.cpp"
    raw = menu_path.read_bytes()
    had_bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig")
    changed = False

    if '#include "font/CJK_Compressed.h"' not in text:
        anchor = '#include "font/Hack_Compressed.h"'
        if anchor in text:
            text = text.replace(anchor, anchor + '\n#include "font/CJK_Compressed.h"', 1)
            changed = True
        else:
            print('警告：找不到 font/Hack_Compressed.h，无法插入 CJK 字体头文件')

    if "// [CN] Merge the embedded CJK subset font" not in text:
        anchor = "\n    if (!Config::Instance()->OverlayMenu.value_or_default())"
        if anchor in text:
            text = text.replace(anchor, "\n" + FONT_BLOCK + anchor.lstrip("\n"), 1)
            changed = True
        else:
            print('警告：找不到字体初始化锚点，无法插入中文字体合并代码')

    if changed:
        menu_path.write_bytes(
            (b"\xef\xbb\xbf" if had_bom else b"") + text.encode("utf-8")
        )
        print(f'已补齐中文字体代码：{menu_path.relative_to(ROOT)}')

    project_path = ROOT / "OptiScaler" / "OptiScaler.vcxproj"
    project_raw = project_path.read_bytes()
    project_had_bom = project_raw.startswith(b"\xef\xbb\xbf")
    project_text = project_raw.decode("utf-8-sig")
    if "/w34996" in project_text:
        patched = re.sub(r"(?<!utf-8 )/w34996", "/utf-8 /w34996", project_text)
        if patched != project_text:
            project_text = patched
            project_path.write_bytes(
                (b"\xef\xbb\xbf" if project_had_bom else b"") + project_text.encode("utf-8")
            )
            print(f'已添加 /utf-8：{project_path.relative_to(ROOT)}')
    elif "/utf-8" not in project_text:
        print('警告：vcxproj 中找不到 /w34996，无法自动添加 /utf-8')


def cmd_validate(_args) -> int:
    dictionary = json.loads(DICT_PATH.read_text(encoding="utf-8"))
    errors: list[str] = []
    for key, value in dictionary.items():
        problem = check_value(key, value)
        if problem:
            errors.append(f"{key!r}: {problem}")

    text, _ = source_text(None)
    items = collect_untranslated(text, dictionary, include_all=False)
    if items:
        errors.append(f"仍有 {len(items)} 条 UI 英文未进入字典，运行 report 查看")

    if errors:
        print("校验失败：")
        for error in errors:
            print(" - " + error)
        return 1
    print(f"校验通过：{len(dictionary)} 条词条，格式符/花括号一致，无遗留 UI 英文")
    return 0


def collect_chars() -> set[str]:
    chars: set[str] = set()
    for path in MENU.glob("*.cpp"):
        for ch in path.read_text(encoding="utf-8-sig"):
            cp = ord(ch)
            if (
                0x20 <= cp <= 0x7E
                or 0x4E00 <= cp <= 0x9FFF
                or 0x3000 <= cp <= 0x303F
                or 0xFF00 <= cp <= 0xFFEF
                or ch in "，。！？：；（）【】《》〈〉「」『』…—–·、％℃×÷＝＋－＊／＃＠＆"
            ):
                chars.add(ch)
    return chars


def find_compiler() -> str | None:
    for name in ("g++", "clang++", "cl"):
        path = shutil.which(name)
        if path:
            return path
    return None


def cmd_font(args) -> int:
    from fontTools import subset
    from fontTools.ttLib import TTFont

    font_path = Path(args.font)
    if not font_path.exists():
        print(f"找不到字体：{font_path}")
        return 1

    chars = collect_chars()
    chars_path = TOOLS / "cjk_chars_v11.txt"
    chars_path.write_text("".join(sorted(chars)), encoding="utf-8")
    print(f"界面字符：{len(chars)} 个 -> {chars_path}")

    font = TTFont(str(font_path), fontNumber=args.font_number)
    opts = subset.Options()
    opts.layout_features = ["*"]
    opts.name_IDs = ["*"]
    opts.name_legacy = True
    opts.name_languages = ["*"]
    opts.hinting = False
    opts.desubroutinize = True
    opts.recalc_bounds = True
    opts.drop_tables += ["DSIG"]
    subsetter = subset.Subsetter(options=opts)
    subsetter.populate(text="".join(chars))
    subsetter.subset(font)
    ttf_path = TOOLS / "msyh_cjk.ttf"
    font.save(str(ttf_path))
    print(f"字体子集：{ttf_path}（{ttf_path.stat().st_size} 字节）")

    cmap = TTFont(str(ttf_path)).getBestCmap()
    missing = [c for c in chars if ord(c) not in cmap]
    if missing:
        print("字体缺少字符：" + "".join(sorted(missing)))
        return 1

    exe = TOOLS / "binary_to_compressed_c.exe"
    if not exe.exists():
        compiler = find_compiler()
        if not compiler:
            print("找不到 g++/clang++/cl，无法生成压缩头文件")
            return 1
        cmd = [compiler, "-O2", "-std=c++17", str(TOOLS / "binary_to_compressed_c.cpp"), "-o", str(exe)]
        subprocess.run(cmd, check=True)

    proc = subprocess.run(
        [str(exe), "-u8", str(ttf_path), "cjk"],
        cwd=ROOT, capture_output=True, check=True,
    )
    body = proc.stdout.splitlines()
    while body and body[0].startswith(b"//"):
        body.pop(0)
    proc_stdout = (
        b"// Embedded CJK font subset for the Chinese UI.\n"
        b"// Generated with binary_to_compressed_c -u8.\n"
        + b"\n".join(body)
        + b"\n"
    )
    header = MENU / "font" / "CJK_Compressed.h"
    header.write_bytes(proc_stdout)
    print(f"压缩头文件：{header}（{header.stat().st_size} 字节）")

    if shutil.which("gcc"):
        test_exe = TOOLS / "test_roundtrip.exe"
        subprocess.run(
            ["gcc", "-O2", str(TOOLS / "test_roundtrip.c"),
             "-I", str(MENU / "font"), "-o", str(test_exe)],
            check=True,
        )
        result = subprocess.run([str(test_exe), str(ttf_path)], check=False)
        if result.returncode != 0:
            print("字体压缩往返测试失败")
            return 1
    return 0


def cmd_upstream(_args) -> int:
    subprocess.run(["git", "fetch", "upstream", "--prune", "--tags"], cwd=ROOT, check=True)
    log = subprocess.run(
        ["git", "log", "--oneline", "--date=short", "--pretty=%h %ad %s", "HEAD..upstream/master"],
        cwd=ROOT, capture_output=True, check=True, text=True, encoding="utf-8",
    ).stdout
    lines = log.strip().splitlines()
    print(f"upstream/master 新增 {len(lines)} 个提交")
    for line in lines[:15]:
        print("  " + line)
    if len(lines) > 15:
        print("  ...")
    print("\n预览新版本待翻译字符串：")
    subprocess.run(
        [sys.executable, __file__, "report", "--ref", "upstream/master", "--out",
         str(TOOLS / "review_upstream.txt")],
        cwd=ROOT, check=False,
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="OptiScaler 汉化维护工具")
    sub = parser.add_subparsers(dest="command", required=True)

    report = sub.add_parser("report", help="列出尚未翻译的界面字符串")
    report.add_argument("--ref", help="从指定 git ref 读取源码，例如 upstream/master")
    report.add_argument("--all", action="store_true", help="同时列出非 UI 英文")
    report.add_argument("--out", help="写入报告文件")
    report.add_argument("--tsv", help="输出待翻译 TSV")
    report.set_defaults(func=cmd_report)

    imp = sub.add_parser("import", help="把 TSV 译文合并进字典")
    imp.add_argument("tsv")
    imp.set_defaults(func=cmd_import)

    apply = sub.add_parser("apply", help="把字典应用到菜单源码")
    apply.set_defaults(func=cmd_apply)

    validate = sub.add_parser("validate", help="检查格式符、花括号和遗留英文")
    validate.set_defaults(func=cmd_validate)

    font = sub.add_parser("font", help="重新生成中文字体子集和压缩头文件")
    font.add_argument("--font", default=r"C:\Windows\Fonts\msyh.ttc")
    font.add_argument("--font-number", type=int, default=0)
    font.set_defaults(func=cmd_font)

    upstream = sub.add_parser("upstream", help="拉取上游并预览新版本待翻译字符串")
    upstream.set_defaults(func=cmd_upstream)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
