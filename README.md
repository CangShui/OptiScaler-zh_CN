# OptiScaler-zh_CN 汉化版

基于 [OptiScaler](https://github.com/optiscaler/OptiScaler) 上游 **2026-11-14 nightly**（commit `992f4e1`，源码版本 `10.0.0.1-dev`）的**界面全量中文化**版本，内置中文字体，开箱即用。

> OptiScaler 是一个跨 GPU 的超分/帧生成桥接工具：可将 DLSS2+/FSR2+/XeSS 等替换为 FSR 3.X/4、XeSS、DLSS 等，并支持帧生成管理。本仓库仅做界面汉化，功能与上游 nightly 一致。

## 📥 下载

请到 **[Releases](../../releases) 页面下载** 编译好的成品压缩包（`OptiScaler_v10.0.0-nightly-20261114_zh-CN.7z`），与官方发布包同布局，解压即用。

## ✅ 汉化内容

- **新版标签页菜单全部中文化**：自定义 / 超分 / 帧生成 / 延迟 / 纹理 / 兼容性 / 菜单 / 兼容性修正 / 状态，以及各标签页内的卡片、按钮、复选框、下拉项、滑块标签、`(?)` 帮助提示和状态文本，共 **1500+ 处**字符串。
- **覆盖上游 10.0 新增界面**：FPS 覆盖层自定义（预设、布局、文本、颜色）、重投影、翻转节流、自定义标签页、设置分享、状态页、光线重建、Vulkan 呈现模式等。
- **安装与卸载脚本中文化**：Windows 安装器、生成的 `Remove_OptiScaler.bat`、Linux 安装/卸载脚本和 Streamline 下载器均为中文交互。
- **保留英文技术名词**：DLSS / DLSS-D / DLSSG / XeSS / XeFG / FSR / OptiScaler / OptiFG / HUDless / HUDFix / fakenvapi / RCAS / DAS / Nvngx / Streamline / PRESET A–O 等，与官方文档、配置项保持一致。
- **内置中文字体**：微软雅黑子集（界面用到的 847 个汉字/标点，196,416 字节 TTF）经 stb_compress 压缩后（169,149 字节）嵌入 DLL，与默认 Hack 字体合并渲染，中英文混排正常，无需放置任何字体文件。

## 📦 使用方法

1. 下载本仓库 Release 中的压缩包，解压后按官方说明部署到游戏目录（将 `OptiScaler.dll` 改名/部署为游戏加载的 dll，通常为 `dxgi.dll` 或 `winmm.dll`）。
2. 进游戏按默认快捷键（Insert）呼出菜单，界面即为中文。
3. 想恢复英文？换回官方原版 `OptiScaler.dll` 即可。汉化**不改变任何配置键名与存储值**，`OptiScaler.ini` 与官方完全兼容。

## 🔧 自行编译

环境：Visual Studio 2022（v143）+ Windows 10/11 SDK。

```bat
git clone --recursive -b main https://github.com/CangShui/OptiScaler-zh_CN.git
cd OptiScaler-zh_CN
powershell -ExecutionPolicy Bypass -File tools_cn\build_windows.ps1 -Package
```

产物位于 `x64\Release\a\OptiScaler.dll`（含全部配套 DLL 的完整发布目录），发布包在 `release\`。

### 源码改动（相对上游 nightly `992f4e1`）

| 文件 | 改动 |
| --- | --- |
| `OptiScaler/menu/menu_common.cpp` | 界面字符串全量汉化；引入 `font/CJK_Compressed.h`；字体初始化合并内嵌中文字体（覆盖所有字体模式） |
| `OptiScaler/menu/font/CJK_Compressed.h` | 新增：微软雅黑子集字体（stb_compress 压缩 + C 数组，169KB） |
| `OptiScaler/OptiScaler.vcxproj` | 编译选项增加 `/utf-8` |

翻译维护脚本见 `tools_cn/`：`dict_v11.json` 为当前字典（1559 条），`apply_dict_v11.py` 执行替换，`build_dict_v11.py` 从 `dict_v11_extra.py` 生成字典。`dict_v10.json` 与旧脚本保留用于追溯上一版翻译。

### 汉化维护与快速同步

安装、卸载和 Streamline 下载脚本已中文化。后续同步上游时不需要逐行重翻 `menu_common.cpp`：

```bat
python tools_cn\localize.py upstream
python tools_cn\localize.py report --tsv tools_cn\review.tsv
python tools_cn\localize.py import tools_cn\review.tsv
python tools_cn\localize.py apply
python tools_cn\localize.py validate
python tools_cn\localize.py font
powershell -ExecutionPolicy Bypass -File tools_cn\build_windows.ps1 -Package
```

`report` 会列出新增的界面英文并给出相近译文，`import` 合并后即可批量替换；`font` 会根据当前界面字符重新裁剪字体，`build_windows.ps1` 负责编译和打包。完整方法论、术语约定和脚本汉化注意事项见 [`tools_cn/README.md`](tools_cn/README.md)。

## ✅ 验证

- 已使用 MSBuild 完成 `Release|x64` 编译，产物为 `x64\Release\a\OptiScaler.dll`。
- 压缩字体数据用 ImGui 同款 `stb_decompress` 解压后与原 TTF **字节级一致**（169,149 → 196,416 字节，往返测试通过，见 `tools_cn/test_roundtrip.c`）。
- 界面用到的全部 847 个中文字符均在字体子集中（`tools_cn/verify_font_chars.py` 校验通过）。
- 汉化不触碰配置读写逻辑：所有下拉框仍按索引/枚举存储，`OptiScaler.ini` 兼容官方。

## ⚠️ 说明

- 菜单字体为 Hack（英文）+ 微软雅黑（中文）合并渲染。
- spdlog 日志输出仍为英文（开发日志，不影响界面）。
- 本仓库是**独立汉化分支**，与官方主仓库无关，请勿向官方提汉化相关的 issue。

## 致谢

- [OptiScaler](https://github.com/optiscaler/OptiScaler) 原项目（MIT License）
- [Dear ImGui](https://github.com/ocornut/imgui)（MIT License）
