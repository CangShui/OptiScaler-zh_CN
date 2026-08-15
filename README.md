# OptiScaler-zh_CN 汉化版

基于 [OptiScaler v0.9.4](https://github.com/optiscaler/OptiScaler/releases/tag/v0.9.4)（commit `7534ad0`）官方源码的**界面全量中文化**版本，并内置中文字体，开箱即用。

> OptiScaler 是一个跨 GPU 的超分/帧生成桥接工具：可将 DLSS2+/FSR2+/XeSS 等替换为 FSR 3.X/4、XeSS、DLSS 等，并支持帧生成管理。本仓库仅做界面汉化，功能与官方 v0.9.4 完全一致。

## 📥 下载

请到 **[Releases](../../releases) 页面下载** 编译好的成品压缩包（`OptiScaler_v0.9.4_汉化版.zip`），与官方发布包同布局，解压即用。

## ✅ 汉化内容

- **游戏内覆盖菜单全部界面**：超分器 / FFX 设置 / 帧生成 / FSR 通用设置 / 视场角与相机数值 / 帧率（fakenvapi）/ 锐化（RCAS、深度感知、运动自适应）/ 超分倍率覆盖 / 输出缩放 / 初始化标志 / 高级设置 / 日志 / 菜单主题与颜色 / FPS 覆盖层 / 超分器输入 / V-Sync 设置 / Mipmap 偏移 / 各向异性过滤 / 按键绑定 / 菜单缩放 / 保存设置 / 关闭 / 打开 Wiki 等全部面板的按钮、复选框、下拉项、滑块标签、`(?)` 帮助提示、状态文本，以及启动画面彩蛋，共 **940+ 处**字符串。
- **保留英文技术名词**：DLSS / DLSS-D / DLSSG / XeSS / XeFG / FSR / OptiScaler / OptiFG / HUDless / HUDFix / fakenvapi / RCAS / DAS / PRESET A–O 等，与官方文档、配置项保持一致。
- **内置中文字体**：微软雅黑子集（界面用到的 715 个汉字/标点，约 493KB）经 stb_compress 压缩后（约 420KB）嵌入 DLL，与默认 Hack 字体合并渲染，中英文混排正常，无需放置任何字体文件。

## 📦 使用方法

1. 下载本仓库 Release 中的压缩包，解压后按官方说明部署到游戏目录（将 `OptiScaler.dll` 改名/部署为游戏加载的 dll，通常为 `dxgi.dll` 或 `winmm.dll`）。
2. 进游戏按默认快捷键（Insert）呼出菜单，界面即为中文。
3. 想恢复英文？换回官方原版 `OptiScaler.dll` 即可。汉化**不改变任何配置键名与存储值**，`OptiScaler.ini` 与官方完全兼容。

## 🔧 自行编译

环境：Visual Studio 2022（v143）+ Windows 10/11 SDK。

```bat
git clone --recursive -b main https://github.com/CangShui/OptiScaler-zh_CN.git
cd OptiScaler-zh_CN
msbuild OptiScaler.sln /p:Configuration=Release /p:Platform=x64
```

产物位于 `x64\Release\a\OptiScaler.dll`（含全部配套 DLL 的完整发布目录）。

### 源码改动（相对官方 v0.9.4）

| 文件 | 改动 |
| --- | --- |
| `OptiScaler/menu/menu_common.cpp` | 界面字符串全量汉化；新增 `font/CJK_Compressed.h` 引入；字体初始化合并内嵌中文字体（覆盖所有字体模式） |
| `OptiScaler/menu/font/CJK_Compressed.h` | 新增：微软雅黑子集字体（stb_compress 压缩 + C 数组，420KB） |
| `OptiScaler/OptiScaler.vcxproj` | 编译选项增加 `/utf-8` |

翻译维护脚本见 `tools_cn/`（字典 `dict.json` + 自动替换脚本），可用于后续版本的汉化更新。

## ✅ 验证

- 压缩字体数据用 ImGui 同款 `stb_decompress` 解压后与原 TTF **字节级一致**（往返测试通过，见 `tools_cn/test_roundtrip.c`）。
- 界面用到的全部 715 个中文字符均在字体子集中（脚本校验通过）。
- 汉化不触碰配置读写逻辑：所有下拉框仍按索引/枚举存储，`OptiScaler.ini` 兼容官方。

## ⚠️ 说明

- 菜单字体为 Hack（英文）+ 微软雅黑（中文）合并渲染。
- spdlog 日志输出仍为英文（开发日志，不影响界面）。
- 本仓库是**独立汉化分支**，与官方主仓库无关，请勿向官方提汉化相关的 issue。

## 致谢

- [OptiScaler](https://github.com/optiscaler/OptiScaler) 原项目（MIT License）
- [Dear ImGui](https://github.com/ocornut/imgui)（MIT License）
