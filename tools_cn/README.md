# OptiScaler 汉化维护指南

这个目录是汉化版的“翻译源 + 工具链”。目标不是逐行改 `menu_common.cpp`，而是维护一份英文到中文的字典，让每次同步上游时只需要处理新增字符串。

## 汉化构成

| 部分 | 文件 | 说明 |
| --- | --- | --- |
| 翻译字典 | `dict_v11.json` | 英文原文 -> 中文译文，当前 1559 条 |
| 新增词条源 | `dict_v11_extra.py` | 本轮同步新增的翻译，`build_dict_v11.py` 会合并 |
| 替换脚本 | `apply_dict_v11.py` | 只替换注释之外的字符串字面量，保留格式符、换行和 BOM |
| 分析/校验 | `localize.py` | 报告、导入、应用、校验、字体、上游预览 |
| 字体 | `OptiScaler/menu/font/CJK_Compressed.h` | 微软雅黑子集，stb_compress 压缩后嵌入 DLL |
| 编译 | `build_windows.ps1` | 自动补齐 Include/Lib 路径并调用 MSBuild |
| 安装/卸载 | `setup_windows.bat`、`setup_linux.sh` | 中文交互；Windows 卸载脚本由安装脚本生成 |

菜单代码只改 `OptiScaler/menu/menu_common.cpp`：

- 引入 `font/CJK_Compressed.h`
- 在 `MenuCommon::Init` 中用 `MergeMode` 合并中文字体
- `OptiScaler.vcxproj` 使用 `/utf-8`

翻译不改变配置键名、枚举存储和 `OptiScaler.ini` 格式。

## 下次同步上游的快速流程

先预览新版本还有多少字符串要翻译，不切换分支：

```bat
python tools_cn\localize.py upstream
```

确认要同步后：

```bat
git fetch upstream --prune --tags
git checkout -B sync-work upstream/master
git checkout main -- tools_cn README.md setup_windows.bat setup_linux.sh Streamlined_fetcher_windows.bat .github/workflows/build.yml .gitignore OptiScaler/menu/font/CJK_Compressed.h
python tools_cn\localize.py apply
python tools_cn\localize.py report --tsv tools_cn\review.tsv
```

`review.tsv` 的第一列是英文原文，第二列是相近译文建议，第三列是上下文。把第二列改成最终中文（没有建议的也补上），然后：

```bat
python tools_cn\localize.py import tools_cn\review.tsv
python tools_cn\localize.py apply
python tools_cn\localize.py validate
python tools_cn\localize.py font
powershell -ExecutionPolicy Bypass -File tools_cn\build_windows.ps1 -Package
```

产物在 `x64\Release\a\`，发布包在 `release\`。确认无误后把 `sync-work` 合并回 `main` 并推送。

## localize.py 命令

```text
python tools_cn\localize.py report [--ref upstream/master] [--tsv review.tsv] [--all] [--out report.txt]
python tools_cn\localize.py import review.tsv
python tools_cn\localize.py apply
python tools_cn\localize.py validate
python tools_cn\localize.py font [--font C:\Windows\Fonts\msyh.ttc]
python tools_cn\localize.py upstream
```

- `report`：只列出尚未进入字典的英文界面字符串，并用 `difflib` 给出相近译文建议。
- `report --ref upstream/master`：不切分支，直接预览上游新版本的待翻译内容。
- `import`：把 TSV 合并进 `dict_v11.json`，自动检查 `%s`/`%d`、`{}` 和空译文。
- `apply`：把字典应用到 `OptiScaler/menu/*.cpp`，可重复执行。
- `validate`：检查字典格式符、花括号、空译文，以及是否还有未翻译的 UI 英文。
- `font`：扫描菜单源码里的全部字符，重新裁剪字体、生成压缩头文件，并用 `stb_decompress` 做往返校验。
- `upstream`：拉取上游并生成 `review_upstream.txt`，用于评估本次同步的工作量。

## 翻译约定

- 保留技术名词：DLSS / DLSSD / DLSSG / FSR / XeSS / XeFG / OptiFG / HUDless / HUDFix / Nvngx / Streamline / RCAS / DAS / PRESET A–O。
- 保留格式符和 ImGui ID：`%s`、`%.2f`、`##2`、`###xefgMode` 原样保留；只翻译 ID 前面的可见文字。
- 下拉框仍按索引/枚举存储，不翻译配置值。
- 同一术语保持统一：Upscaler=超分器、Frame Gen=帧生成、Sharpness=锐化、Output Scaling=输出缩放、Low Latency=低延迟。
- 状态页和帮助提示可以意译，但不要改变“开启/关闭”“兼容/不兼容”的含义。

## 安装脚本汉化注意事项

- Windows 批处理保存为 **UTF-8 无 BOM**，开头加 `chcp 65001 >nul`。
- 延迟展开开启时，提示文字不要用半角 `!`，用全角 `！` 避免被当作变量展开。
- 提示文字里的括号用全角 `（）`，避免被 `if (...)` 块误解析。
- 生成的 `Remove_OptiScaler.bat` 也要写 `chcp 65001 >nul`，保证卸载界面中文正常。
- `pause` 用 `echo 按任意键继续……` + `pause >nul`，避免出现英文系统提示。
- Linux 脚本保持 UTF-8 和 LF；改完用 `bash -n setup_linux.sh` 做语法检查。

## 校验与回归

每次发版至少跑一遍：

```bat
python tools_cn\localize.py validate
python tools_cn\localize.py font
powershell -ExecutionPolicy Bypass -File tools_cn\build_windows.ps1
```

`font` 会输出压缩前后字节数并执行往返测试；`build_windows.ps1` 会检查 DLL 是否生成。安装脚本改动较大时，在临时目录放一个空的 `OptiScaler.dll`，分别走一遍安装和卸载流程，确认没有语法错误。
