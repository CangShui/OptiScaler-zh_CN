#include "pch.h"
#include "menu_common.h"
#include "PresentRateMeter.h"
#include <inputs/FG/XeFG_Inputs_Dx12.h>

#include "input/input_system.h"

#include "font/Hack_Compressed.h"
#include "font/CJK_Compressed.h"

#include <proxies/XeSS_Proxy.h>
#include <proxies/XeFG_Proxy.h>
#include <proxies/FfxApi_Proxy.h>
#include <upscalers/ffx/FFXFeature.h>
#include <upscalers/dlssd/DLSSDFeature.h>
#include <fsr4/FSR4Models.h>
#include <fsr4/FSR4Presets.h>
#include <fsr4/FSR4DriverProviderVk.h>
#include <proxies/Streamline_Proxy.h>

#include <framegen/nvngx/Nvngx_FG.h>
#include <upscalers/nvngx/Nvngx_RR.h>
#include <framegen/reprojection/Reprojection_Dx12.h>
#include <framegen/flipmeter/FlipMeterFG_Dx12.h>
#include <framegen/flipmeter/FlipMeterSwapChain.h>

#include <nvapi/fakenvapi.h>
#include <hooks/Reflex_Hooks.h>
#include <hooks/Vulkan_Hooks.h>

#include <BuildInfo.h>
#include <version_check.h>

#include <imgui/imgui_internal.h>
#include <imgui/ImGuiNotify.hpp>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_uwp.h>

#include <mutex>
#include <cstdarg>
#include <sstream>

#include <array>
#include <numeric>
#include <chrono>
#include <memory>
#include <type_traits>
#include <misc/IdentifyGpu.h>
#include <hooks/Amdxc64_Hooks.h>
#include <low_latency/input/input_common.h>
#include <plugins/PluginApi.h>
#include <hooks/Vulkan_Hooks.h>
#include <low_latency/input/input_xell.h>

enum class UiTargetMode
{
    SDR,
    LinearHDR,
    ScRGB,
    PQ,
    HLG
};

#define MARK_ALL_BACKENDS_CHANGED()                                                                                    \
    for (auto& singleChangeBackend : State::Instance().changeBackend)                                                  \
        singleChangeBackend.second = true;

static float fontSize = 14.0f; // just changing this doesn't make other elements scale ideally
static ImVec2 overlaySize(0.0f, 0.0f);
static ImVec2 overlayPosition(-1000.0f, -1000.0f);
static bool _hdrTonemapApplied = false;
static ImVec4 SdrColors[ImGuiCol_COUNT];

static bool inputMenu = false;
static bool inputFG = false;
static bool fgHotkeyLeftDefault = false; // FG hotkey temporarily overrode Default (follow game)
static int fgHotkeySavedOverride = -1;   // Override DLSSG Ratio with value before Off, -1 = Default
static bool inputFps = false;
static bool inputFpsCycle = false;
static uint64_t lastInputTick = 0;
constexpr uint64_t debounceThreshold = 1000;

static bool hasGamepad = false;
static bool ffxInitTried = false;
static bool xefgInitTried = false;
static std::string windowTitle;
static std::string selectedUpscalerName = "";
static Upscaler currentBackend = Upscaler::Reset;
static std::string currentBackendName = "";
static int refreshRate = 0;
static ImVec2 lastPosition(-1000.0f, -1000.0f);

static ImVec2 splashPosition(-1000.0f, -1000.0f);
static ImVec2 splashSize(0.0f, 0.0f);
static double splashStart = 0.0;
static double splashLimit = 0.0;
static std::vector<std::string> splashText = { "聪明地应对，而不是硬扛",
                                               "这位的应对能力很强……",
                                               "乐趣从此开始……",
                                               "还有更多的缩放器吗？……",
                                               "假像素，更假的帧……",
                                               "假帧，快来看假帧……",
                                               "我来踢像素、嚼帧数……",
                                               "我发现你缺乏超采样……",
                                               "一帧一帧，我把画面放大！",
                                               "抵抗是徒劳的。你的像素将被超分。",
                                               "我有 99 个问题，但低分辨率不算。",
                                               "结束了，DLSS，我占据了高地！",
                                               "这不是你要找的分辨率",
                                               "飞向无限……关掉光追的那种",
                                               "我对这个帧生成节奏有种不祥的预感",
                                               "独自前行很危险——带上这个超分器",
                                               "超分到面目全非。",
                                               "相信过程，无视闪烁。",
                                               "真正的假帧。经过认证。",
                                               "性能的幻觉",
                                               "这个超分器应该进博物馆！",
                                               "因为原生渲染被高估了。",
                                               "超分越多，省得越多。",
                                               "买更好的显卡永远不晚。",
                                               "我们去的地方不需要真实像素。",
                                               "你知道吗，Intel 向所有人发布了 XeFG？",
                                               "MFG 与 Nukem 的配合 100%% 有效，绝非骗局",
                                               "其中有些像素甚至可能是真的！",
                                               "别太仔细看画面。",
                                               "甚至支持“软件”版 XeSS！",
                                               "太模糊了，带上 RCAS 再上路",
                                               "感谢 nitec，交还给 nitec",
                                               "由 By-U 测试并认可",
                                               "0.8 是内部阴谋",
                                               "FSR4 DP4a 何时出，AMD 求求了",
                                               "Opti 自救队，集合！",
                                               "这才是超分应有的样子",
                                               "今天你的游戏可能都不崩溃",
                                               "扩展与增强",
                                               "今天才崩第 5 次",
                                               "帧生成有延迟？可我的网很好啊",
                                               "主机党可做不到这个",
                                               "希望你视力不好",
                                               "这么激进的超分？大胆的决定",
                                               "我几乎感觉不到输入延迟",
                                               "这就是你达到 60 帧的方式",
                                               "我们一起超分",
                                               "为了超分器，由超分器创造",
                                               "Opti 体育，采样就在其中",
                                               "在你的世界里渲染，在我们的世界里超分",
                                               "你的所有像素都归我们了",
                                               "为大众超分，而非少数人",
                                               "自 2023 年起制造纷争",
                                               "自 2023 年起为 DLSS 赋能",
                                               "[已删除]从未如此好看",
                                               "免费，永远免费",
                                               "正在摆脱绿厂锁链……",
                                               "Nukem 到底是谁？",
                                               "正在编译着色器……预计：05 小时 49 分",
                                               "你真的花了 70 欧买这游戏？！",
                                               "猜猜谁又忘了检查空指针",
                                               "AI 也糊不过这个",
                                               "看来我们现在是 pre-alpha 演示版了",
                                               "街区新面孔 - TH",
                                               "再卡一次我就要疯了",
                                               "大体稳定，不像驱动那样",
                                               "Vul……啥？~AMD",
                                               "我的 8 个点正在浮动",
                                               "这里没有浮点——我严格在 -128 到 127 之间",
                                               "先假装做到，直到真的做到",
                                               "最坏情况就关了再开",
                                               "*几何层面的生成式损害控制模式*",
                                               "深度学习糊弄采样 5",
                                               "2D AI 滤镜，仅需 2 张 5090 驱动",
                                               "神经糊弄采样搭配 DLSS5",
                                               "DLSS 5——糊弄的正确方式",
                                               "每当我以为退坑了，他们又把我拉回来超分",
                                               "就像在高速上挂一档",
                                               "Nitec 的奇妙超分",
                                               "“帧生成真是吸引了一些奇怪的顾客”",
                                               "怎么去掉这些土味消息？！",
                                               "我甚至不知道我在这里做什么",
                                               "别再调设置了，快去玩吧",
                                               "哦太好了，又一台土豆电脑",
                                               "XeFG 是国王，但谁是王后？",
                                               "原生分辨率？你肯定在开玩笑",
                                               "享受你的假帧吧",
                                               "别看这条",
                                               "你知道我能看到你的 GPU 吧？",
                                               "这游戏的帧生成要加班了",
                                               "嗨",
                                               "很高兴见到你",
                                               "你好吗？",
                                               "如果游戏崩溃了，那可不是我们的错",
                                               "<在这里填入你的搞笑文字>" };

static std::string updateNoticeTag;
static std::string updateNoticeUrl;
static float lastMenuScale = 0.0f;
static CustomOptional<uint32_t> comboPreset { 0 };
static int lastKey = 0;
static bool capturingKey = false;
static int selectedMenuTab = 0;
static const char* requestedMenuTab = nullptr; // Label of a tab to switch to on the next frame

template <typename T, size_t N> struct RingBuffer
{
    std::array<T, N> data {};
    size_t head { 0 };
    size_t count { N };
    double sum { 0.0 };

    RingBuffer() { data.fill(static_cast<T>(0)); }

    void Push(T v)
    {
        if (count == N)
        {
            sum -= data[head];
        }
        else
        {
            ++count;
        }
        data[head] = v;
        sum += v;
        head = (head + 1) % N;
    }

    size_t Size() const { return N; }

    T At(size_t i) const
    {
        size_t start = head;
        return data[(start + i) % N];
    }

    float Average() const { return static_cast<float>(sum / static_cast<double>(N)); }
};

const int plotWidth = 360;
static RingBuffer<float, plotWidth> gFrameTimes;
static RingBuffer<float, plotWidth> gUpscalerTimes;

// FPS overlay fonts offered in the menu, from the Windows fonts folder
static const std::pair<const char*, const char*> overlayFonts[] = {
    { "Segoe UI", "segoeui.ttf" },
    { "Segoe UI Semilight", "segoeuisl.ttf" },
    { "Segoe UI Semibold", "seguisb.ttf" },
    { "Segoe UI Bold", "segoeuib.ttf" },
    { "Segoe UI Italic", "segoeuii.ttf" },
    { "Segoe UI Black", "seguibl.ttf" },
    { "Segoe UI Variable", "SegUIVar.ttf" },
    { "Bahnschrift", "bahnschrift.ttf" },
    { "Consolas", "consola.ttf" },
    { "Consolas Bold", "consolab.ttf" },
    { "Consolas Italic", "consolai.ttf" },
    { "Cascadia Mono", "CascadiaMono.ttf" },
    { "Cascadia Code", "CascadiaCode.ttf" },
    { "Unispace Bold", "unispace bd.ttf" },
    { "Lucida Console", "lucon.ttf" },
    { "Lucida Sans Unicode", "l_10646.ttf" },
    { "Courier New", "cour.ttf" },
    { "Courier New Bold", "courbd.ttf" },
    { "Arial", "arial.ttf" },
    { "Arial Bold", "arialbd.ttf" },
    { "Arial Black", "ariblk.ttf" },
    { "Verdana", "verdana.ttf" },
    { "Verdana Bold", "verdanab.ttf" },
    { "Tahoma", "tahoma.ttf" },
    { "Tahoma Bold", "tahomabd.ttf" },
    { "Calibri", "calibri.ttf" },
    { "Calibri Bold", "calibrib.ttf" },
    { "Trebuchet MS", "trebuc.ttf" },
    { "Trebuchet MS Bold", "trebucbd.ttf" },
    { "Century Gothic", "GOTHIC.TTF" },
    { "Century Gothic Bold", "GOTHICB.TTF" },
    { "Franklin Gothic Medium", "framd.ttf" },
    { "Franklin Gothic Demi", "FRADM.TTF" },
    { "Corbel", "corbel.ttf" },
    { "Corbel Bold", "corbelb.ttf" },
    { "Candara", "Candara.ttf" },
    { "Candara Bold", "Candarab.ttf" },
    { "Constantia", "constan.ttf" },
    { "Constantia Bold", "constanb.ttf" },
    { "Palatino Linotype", "pala.ttf" },
    { "Palatino Linotype Bold", "palab.ttf" },
    { "Georgia", "georgia.ttf" },
    { "Georgia Bold", "georgiab.ttf" },
    { "Garamond", "GARA.TTF" },
    { "Garamond Bold", "GARABD.TTF" },
    { "Sitka Text", "Sitka.ttc" },
    { "Sylfaen", "sylfaen.ttf" },
    { "Times New Roman", "times.ttf" },
    { "Times New Roman Bold", "timesbd.ttf" },
    { "Comic Sans MS", "comic.ttf" },
    { "Comic Sans MS Bold", "comicbd.ttf" },
    { "Impact", "impact.ttf" },
};

// FPS overlay rows, FpsOverlayRows lists their keys. A graph row follows its text row.
enum OverlayRow : int
{
    OverlayRow_Fps,
    OverlayRow_Upscale,
    OverlayRow_FrameTime,
    OverlayRow_FrameTimeGraph,
    OverlayRow_UpscalerTime,
    OverlayRow_UpscalerTimeGraph,
    OverlayRow_LowLatency,
    OverlayRow_COUNT,
};

static const char* overlayRowKeys[] = { "fps",      "upscale",       "frametime", "frametimegraph",
                                        "upscaler", "upscalergraph", "lowlatency" };
static const char* overlayRowNames[] = {
    "FPS",           "超分倍率",       "帧时间",         "帧时间图表",
    "超分器耗时", "超分器耗时图表", "低延迟计时"
};
static const char* overlayRowTips[] = {
    "FPS、帧生成与超分器；下方 FPS 行决定显示内容",
    "超分时的渲染分辨率 -> 输出分辨率与超分倍率",
    "上一帧时间与平均值",
    "最近若干帧的帧时间",
    "超分器上一帧耗时与平均值",
    "最近若干帧的超分器耗时",
    "有低延迟标记时，显示该帧的计时",
};

struct OverlayRowEntry
{
    int row;
    bool shown;
};

// An overlay type's rows from top to bottom (left to right when horizontal), the hidden ones too so Custom keeps
// their place
static std::vector<OverlayRowEntry> OverlayRows(FpsOverlay type)
{
    auto config = Config::Instance();

    if (type != FpsOverlay_Custom)
    {
        // The lowest type that shows the row, each type shows more than the one before
        static constexpr FpsOverlay shownFrom[] = {
            FpsOverlay_JustFPS, FpsOverlay_Simple,    FpsOverlay_Detailed,         FpsOverlay_DetailedGraph,
            FpsOverlay_Full,    FpsOverlay_FullGraph, FpsOverlay_LowLatencyTimings
        };
        static_assert(std::size(shownFrom) == OverlayRow_COUNT);

        std::vector<OverlayRowEntry> rows;

        for (int row = 0; row < OverlayRow_COUNT; row++)
        {
            bool shown = type >= shownFrom[row];

            if (row == OverlayRow_Upscale)
                shown = shown && config->FpsOverlayShowRatio.value_or_default();

            rows.push_back({ row, shown });
        }

        return rows;
    }

    if (!config->FpsOverlayRows.has_value())
        return OverlayRows(FpsOverlay_FullGraph);

    std::vector<OverlayRowEntry> rows;
    std::stringstream keys(config->FpsOverlayRows.value());
    std::string key;

    auto listed = [&](int row)
    { return std::any_of(rows.begin(), rows.end(), [row](const OverlayRowEntry& entry) { return entry.row == row; }); };

    while (std::getline(keys, key, ','))
    {
        std::string_view name = key;
        while (!name.empty() && std::isspace((unsigned char) name.front()))
            name.remove_prefix(1);
        while (!name.empty() && std::isspace((unsigned char) name.back()))
            name.remove_suffix(1);

        const bool shown = !name.starts_with('-');
        if (!shown)
            name.remove_prefix(1);

        for (int row = 0; row < OverlayRow_COUNT; row++)
        {
            if (name == overlayRowKeys[row] && !listed(row))
                rows.push_back({ row, shown });
        }
    }

    for (int row = 0; row < OverlayRow_COUNT; row++)
    {
        if (!listed(row))
            rows.push_back({ row, false });
    }

    return rows;
}

static void SetOverlayRows(const std::vector<OverlayRowEntry>& rows)
{
    std::string keys;

    for (const auto& entry : rows)
        keys += (keys.empty() ? "" : ",") + std::string(entry.shown ? "" : "-") + overlayRowKeys[entry.row];

    Config::Instance()->FpsOverlayRows = keys;
}

static FpsOverlayFpsRow OverlayFpsRow(FpsOverlay type)
{
    switch (type)
    {
    case FpsOverlay_JustFPS:
        return FpsOverlayFpsRow_JustFps;

    case FpsOverlay_Simple:
        return FpsOverlayFpsRow_FrameTime;

    case FpsOverlay_Custom:
        return Config::Instance()->FpsOverlayFpsRowType.value_or_default();

    default:
        return FpsOverlayFpsRow_Average;
    }
}

// The digits each number of an FPS overlay value needs to keep the most it had this session, so values start as far
// left as they can and don't move back and forth as they change
static std::vector<int> OverlayValuePadding(const char* id, std::string_view text)
{
    static std::unordered_map<std::string, std::vector<int>> mostDigits;
    auto& most = mostDigits[id];

    std::vector<int> padding;
    size_t i = 0;

    while (i < text.size())
    {
        if (text[i] < '0' || text[i] > '9')
        {
            i++;
            continue;
        }

        const size_t start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            i++;

        const int digits = static_cast<int>(i - start);
        const size_t number = padding.size();

        if (number == most.size())
            most.push_back(digits);
        else
            most[number] = std::max(most[number], digits);

        padding.push_back(most[number] - digits);
    }

    return padding;
}

// FPS overlay font, loaded between frames when FpsOverlayFont or its rasterizer settings change
static ImFont* gOverlayFont = nullptr;
static std::string gOverlayFontName;
static float gOverlayLetterSpacing = 0.0f;

static std::filesystem::path OverlayFontPath(const std::string& name)
{
    std::filesystem::path path(name);

    if (path.has_parent_path())
        return path;

    static const std::filesystem::path fontsDir = []()
    {
        wchar_t windowsDir[MAX_PATH] {};
        GetWindowsDirectoryW(windowsDir, MAX_PATH);
        return std::filesystem::path(windowsDir) / L"Fonts";
    }();

    return fontsDir / path;
}

static void UpdateOverlayFont()
{
    auto config = Config::Instance();
    auto wanted = config->FpsOverlayFont.value_or("");
    const float letterSpacing = config->FpsOverlayLetterSpacing.value_or_default();

    if (wanted == gOverlayFontName && std::abs(letterSpacing - gOverlayLetterSpacing) < 0.001f)
    {
        return;
    }

    gOverlayFontName = wanted;
    gOverlayLetterSpacing = letterSpacing;

    auto& io = ImGui::GetIO();

    if (gOverlayFont != nullptr)
    {
        io.Fonts->RemoveFont(gOverlayFont);
        gOverlayFont = nullptr;
    }

    if (wanted.empty() && std::abs(letterSpacing) < 0.01f)
        return;

    // The menu keeps its font, the first one added would become the default
    if (io.FontDefault == nullptr)
        io.FontDefault = io.Fonts->Fonts.empty() ? io.Fonts->AddFontDefault() : io.Fonts->Fonts[0];

    ImFontConfig fontConfig;
    fontConfig.GlyphExtraAdvanceX = letterSpacing;

    if (!wanted.empty())
    {
        auto path = OverlayFontPath(wanted);

        if (!std::filesystem::exists(path))
        {
            LOG_WARN("FPS overlay font not found: {}", wstring_to_string(path.wstring()));
            return;
        }

        gOverlayFont = io.Fonts->AddFontFromFileTTF(wstring_to_string(path.wstring()).c_str(), fontSize, &fontConfig);
    }
    else if (config->UseHQFont.value_or_default())
    {
        if (config->TTFFontPath.has_value())
        {
            gOverlayFont = io.Fonts->AddFontFromFileTTF(wstring_to_string(config->TTFFontPath.value()).c_str(),
                                                        fontSize, &fontConfig, io.Fonts->GetGlyphRangesDefault());
        }
        else
        {
            gOverlayFont = io.Fonts->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                          fontSize, &fontConfig);
        }
    }
}

// FPS overlay colors are stored as 0xRRGGBB
static ImVec4 RgbToColor(uint32_t rgb)
{
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, 1.0f);
}

static uint32_t ColorToRgb(const float* color)
{
    auto channel = [](float c) { return (uint32_t) std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f); };
    return (channel(color[0]) << 16) | (channel(color[1]) << 8) | channel(color[2]);
}

struct FsExistsCache
{
    std::wstring lastPath;
    bool cached { false };
    std::chrono::steady_clock::time_point nextRefresh { std::chrono::steady_clock::time_point::min() };
    std::chrono::milliseconds interval { 2000 };

    bool Get(const std::filesystem::path& path)
    {
        auto now = std::chrono::steady_clock::now();
        if (path != lastPath || now >= nextRefresh)
        {
            lastPath = path;
            cached = std::filesystem::exists(path);
            nextRefresh = now + interval;
        }
        return cached;
    }
};

static FsExistsCache nukemsExists;
static FsExistsCache enablerExists;

struct FlagDefinition
{
    std::string name;
    uint32_t mask;
    std::string description;
};

inline std::string StrFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    std::string out(len, '\0');
    va_start(args, fmt);
    std::vsnprintf(out.data(), len + 1, fmt, args);
    va_end(args);
    return out;
}

static std::string NameWithVersion(IFeature* feature)
{
    const auto version = feature->Version();
    return StrFmt("%s %u.%u.%u", feature->ShortName().c_str(), version.major, version.minor, version.patch);
}

// CARDS -----------------------------
static void DrawCardTitle(const char* label)
{
    ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));

    // Spacing below is not an item the tooltips should attach to
    const ImRect titleRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    const ImGuiID titleId = ImGui::GetItemID();

    ImGui::Spacing();

    // HoveredRect has to reflect the mouse, IsItemHovered trusts it
    const ImGuiItemStatusFlags hovered =
        ImGui::IsMouseHoveringRect(titleRect.Min, titleRect.Max) ? ImGuiItemStatusFlags_HoveredRect : 0;
    ImGui::SetLastItemData(titleId, 0, hovered, titleRect);
}

// Used instead of ImGui::SeparatorText. The first one in a card becomes the title of the card
static void SectionTitle(const char* label)
{
    if (!ScopedCard::TryTitle(label))
        ImGui::SeparatorText(label);
}

ScopedCard::ScopedCard(const char* title)
{
    // Cards don't nest, an inner one only gives a title to the outer one
    if (_current != nullptr)
    {
        if (title != nullptr)
            SectionTitle(title);

        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();

    _window = ImGui::GetCurrentWindow();
    _pad = ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f);
    _min = _window->DC.CursorPos;
    _width = ImGui::GetContentRegionAvail().x;

    // Content goes to the channel 1, the background is added to the channel 0 once the height is known
    _splitter.Split(_window->DrawList, 2);
    _splitter.SetCurrentChannel(_window->DrawList, 1);

    _window->DC.CursorPos.y += _pad.y;
    ImGui::Indent(_pad.x);

    // GetContentRegionAvail (used by child windows) reads ContentRegionRect, so both need the padding
    _oldWorkRectMaxX = _window->WorkRect.Max.x;
    _oldContentRectMaxX = _window->ContentRegionRect.Max.x;
    _window->WorkRect.Max.x -= _pad.x;
    _window->ContentRegionRect.Max.x -= _pad.x;

    _contentStartY = _window->DC.CursorPos.y;
    _active = true;
    _current = this;

    if (title != nullptr)
        DrawTitle(title);
}

void ScopedCard::DrawTitle(const char* label)
{
    // Title sits on the top border, like a group box
    _window->DC.CursorPos.y = _min.y;
    DrawCardTitle(label);

    _titleMin = ImGui::GetItemRectMin();
    _titleMax = ImGui::GetItemRectMax();
    _hasTitle = true;
}

ScopedCard::~ScopedCard()
{
    if (!_active)
        return;

    _current = nullptr;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* drawList = _window->DrawList;

    ImGui::Unindent(_pad.x);
    _window->WorkRect.Max.x = _oldWorkRectMaxX;
    _window->ContentRegionRect.Max.x = _oldContentRectMaxX;

    // Nothing was drawn, skip the rectangle
    if (_window->DC.CursorPos.y <= _contentStartY)
    {
        _splitter.Merge(drawList);
        _window->DC.CursorPos.y = _min.y;
        return;
    }

    const float bottom = _window->DC.CursorPos.y - style.ItemSpacing.y + _pad.y;
    const ImVec2 max(_min.x + _width, bottom);

    _splitter.SetCurrentChannel(drawList, 0);

    const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Separator);

    if (_hasTitle)
    {
        // Same half pixel offsets as AddRect. Top line goes through the middle of the title, with a gap for it
        const float r = style.ChildRounding;
        const float gap = style.ItemInnerSpacing.x;
        const ImVec2 a(_min.x + 0.5f, std::floor((_titleMin.y + _titleMax.y) * 0.5f) + 0.5f);
        const ImVec2 b(max.x - 0.49f, max.y - 0.49f);

        drawList->PathLineTo(ImVec2(_titleMax.x + gap, a.y));
        drawList->PathArcToFast(ImVec2(b.x - r, a.y + r), r, 9, 12);
        drawList->PathArcToFast(ImVec2(b.x - r, b.y - r), r, 0, 3);
        drawList->PathArcToFast(ImVec2(a.x + r, b.y - r), r, 3, 6);
        drawList->PathArcToFast(ImVec2(a.x + r, a.y + r), r, 6, 9);
        drawList->PathLineTo(ImVec2(_titleMin.x - gap, a.y));
        drawList->PathStroke(borderColor, ImDrawFlags_None, 1.0f);
    }
    else
    {
        drawList->AddRect(_min, max, borderColor, style.ChildRounding);
    }

    _splitter.Merge(drawList);

    // Registers the size of the card and leaves a gap before the next one
    _window->DC.CursorPos.y = bottom;
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

bool ScopedCard::TryTitle(const char* label)
{
    if (_current == nullptr || _current->_hasTitle)
        return false;

    // Only when it's the very first thing in the card
    if (ImGui::GetCurrentWindow() != _current->_window || _current->_window->DC.CursorPos.y != _current->_contentStartY)
        return false;

    _current->DrawTitle(label);

    return true;
}

bool ScopedCard::Shows(const char* id)
{
    // Inside of a card everything belongs to that card
    return _filter == nullptr || _current != nullptr || strcmp(_filter, id) == 0;
}

void ScopedCard::SetFilter(const char* id) { _filter = id; }

void MenuCommon::RenderCard(RenderMenuContext& ctx, void (*render)(RenderMenuContext&))
{
    ScopedCard card;
    render(ctx);
}

void MenuCommon::UpdateManualInput(HWND targetHwnd)
{
    OptiInput::BeginFrame(targetHwnd);

    const auto config = Config::Instance();

    auto CheckShortcut = [&](int vk, bool& inputFlag, const char* logMessage)
    {
        if (inputFlag)
            return;

        if (vk <= 0 || vk >= 256)
            return;

        if (OptiInput::IsKeyReleased(vk))
        {
            lastKey = vk;
            // receivingWmInputs = false;
            inputFlag = true;
            LOG_DEBUG("{}", logMessage);
        }
    };

    const auto currentTick = GetTickCount64();
    const bool canAcceptInputs = lastInputTick + debounceThreshold < currentTick;

    if (!capturingKey && canAcceptInputs)
    {
        CheckShortcut(config->ShortcutKey.value_or_default(), inputMenu, "菜单键已按下，将切换到菜单");
        CheckShortcut(config->FpsShortcutKey.value_or_default(), inputFps, "菜单键已按下，将切换到 FPS");
        CheckShortcut(config->FGShortcutKey.value_or_default(), inputFG, "菜单键已按下，将切换到 FG 模式");
        CheckShortcut(config->FpsCycleShortcutKey.value_or_default(), inputFpsCycle,
                      "菜单键已按下，将切换到 FPS 模式");
    }
    else if (capturingKey)
    {
        lastInputTick = currentTick;
    }

    lastKey = OptiInput::GetLastPressedKey();
}

void MenuCommon::ShowTooltip(const char* tip)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::BeginTooltip();
        ImGui::Text(tip);
        ImGui::EndTooltip();
    }
}

template <typename TOption>
bool MenuCommon::ConfigCheckbox(const char* label, TOption& option, const char* tip, bool needsRestart)
{
    bool value = option.value_or_default();
    const bool changed = ImGui::Checkbox(label, &value);

    if (changed)
        option = value;

    if (tip != nullptr)
    {
        if (needsRestart)
            ShowTooltip((std::string(tip) + "\n\n更改此选项后，请保存设置。\n"
                                            "它将在下次启动时生效。")
                            .c_str());
        else
            ShowTooltip(tip);
    }

    return changed;
}

void MenuCommon::ShowResetButton(CustomOptional<bool, NoDefault>* initFlag, std::string buttonName)
{
    ImGui::SameLine();

    ImGui::BeginDisabled(!initFlag->has_value());

    if (ImGui::Button(buttonName.c_str()))
    {
        initFlag->reset();
        ReInitUpscaler();
    }

    ImGui::EndDisabled();
}

inline void MenuCommon::ReInitUpscaler()
{
    if (!State::Instance().currentFeature)
        return;

    // currentBackend is only updated while the menu is drawn, ray reconstruction keeps its DLSSD
    auto feature = State::Instance().currentFeature;
    State::Instance().newBackend =
        State::Instance().rrHandles.contains(feature->Handle()->Id) ? Upscaler::Reset : feature->GetUpscalerType();

    MARK_ALL_BACKENDS_CHANGED();
}

void MenuCommon::SeparatorWithHelpMarker(const char* label, const char* tip)
{
    SectionTitle(label);
    ShowTooltip(tip);
}

bool MenuCommon::SliderUInt(const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max, const char* format,
                            ImGuiSliderFlags flags)
{
    return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
}

class Keybind
{
    std::string name;
    int id;
    bool waitingForKey = false;

  public:
    Keybind(std::string name, int id) : name(name), id(id) {}

    static std::string KeyNameFromVirtualKeyCode(USHORT virtualKey)
    {
        if (virtualKey == (USHORT) UnboundKey)
            return "未绑定";

        UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);

        // Keys like Home would display as Num 0 without this fix
        switch (virtualKey)
        {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
        case VK_RCONTROL:
        case VK_RMENU:
            scanCode |= 0xE000;
            break;
        }

        LONG lParam = (scanCode & 0xFF) << 16;
        if (scanCode & 0xE000)
            lParam |= 1 << 24;

        wchar_t buf[64] = {};
        if (GetKeyNameTextW(lParam, buf, static_cast<int>(std::size(buf))) != 0)
            return wstring_to_string(buf);

        return "未知";
    }

    void Render(CustomOptional<int>& configKey)
    {
        ImGui::PushID(id);
        if (ImGui::Button(name.c_str()))
        {
            waitingForKey = true;
            capturingKey = true;
            lastKey = 0;
        }
        ImGui::PopID();

        if (waitingForKey)
        {
            ImGui::SameLine();
            ImGui::Text("请按任意键……");

            if (lastKey == 0 || lastKey == VK_LBUTTON || lastKey == VK_RBUTTON || lastKey == VK_MBUTTON)
                return;

            if (lastKey == VK_ESCAPE)
            {
                waitingForKey = false;
                capturingKey = false;
                return;
            }

            if (lastKey == VK_BACK)
                lastKey = UnboundKey;

            configKey = lastKey;
            waitingForKey = false;
            capturingKey = false;
            return;
        }

        ImGui::SameLine();
        ImGui::Text(KeyNameFromVirtualKeyCode(configKey.value_or_default()).c_str());

        ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("R"))
        {
            configKey.reset();
        }
        ImGui::PopID();
    }
};

Upscaler MenuCommon::GetBackendCode(const API api)
{
    if (auto feature = State::Instance().currentFeature)
        return feature->GetUpscalerType();

    Upscaler upscaler;

    if (api == DX11)
        upscaler = Config::Instance()->Dx11Upscaler.value_or_default();
    else if (api == DX12)
        upscaler = Config::Instance()->Dx12Upscaler.value_or_default();
    else
        upscaler = Config::Instance()->VulkanUpscaler.value_or_default();

    return upscaler;
}

void MenuCommon::GetCurrentBackendInfo(const API api, Upscaler& upscaler, std::string* name)
{
    upscaler = GetBackendCode(api);
    *name = UpscalerDisplayName(upscaler, api);
}

// Backend that recreates the current feature, ray reconstruction keeps its DLSSD and the upscalers their own
static Upscaler SameBackend(Upscaler currentBackend)
{
    auto feature = State::Instance().currentFeature;

    if (feature != nullptr && State::Instance().rrHandles.contains(feature->Handle()->Id))
        return Upscaler::Reset;

    return currentBackend;
}

// Loaded custom FFX upscaler, nullptr for the regular ones. Only native Vulkan FFX runs the Vulkan dll.
static CustomFfxModule* SelectedCustomFfx(const std::string& name, Upscaler upscaler, API api)
{
    if (name.empty())
        return nullptr;

    return api == API::Vulkan && upscaler == Upscaler::FFX ? FfxApiProxy::FindCustomUpscalerVk(name)
                                                           : FfxApiProxy::FindCustomUpscalerDx12(name);
}

// List of upscalers reported by FFX for a given backend, nullptr if the backend doesn't use FFX
static State::FfxVersionList* GetFfxVersionList(Upscaler upscaler, API api)
{
    if (upscaler == Upscaler::FFX_on12 || (upscaler == Upscaler::FFX && api == API::DX12))
        return &State::Instance().ffxUpscalerVersionsDx12;

    if (upscaler == Upscaler::FFX && api == API::Vulkan)
        return &State::Instance().ffxUpscalerVersionsVk;

    return nullptr;
}

static bool IsOn12(Upscaler upscaler)
{
    switch (upscaler)
    {
    case Upscaler::XeSS_on12:
    case Upscaler::FSR21_on12:
    case Upscaler::FSR22_on12:
    case Upscaler::FFX_on12:
    case Upscaler::DLSS_on12:
        return true;
    default:
        return false;
    }
}

// Name without the w/Dx12 suffix, that one is shown as a tag in the list instead
static std::string ShortUpscalerName(Upscaler upscaler, API api, int ffxIndex)
{
    if (auto versions = GetFfxVersionList(upscaler, api);
        versions != nullptr && ffxIndex >= 0 && ffxIndex < versions->names.size())
        return StrFmt("FSR %s", versions->names[ffxIndex]);

    switch (upscaler)
    {
    case Upscaler::XeSS_on12:
        return UpscalerDisplayName(Upscaler::XeSS);
    case Upscaler::FSR21_on12:
        return UpscalerDisplayName(Upscaler::FSR21);
    case Upscaler::FSR22_on12:
        return UpscalerDisplayName(Upscaler::FSR22);
    case Upscaler::FFX_on12:
        return UpscalerDisplayName(Upscaler::FFX, API::DX12);
    case Upscaler::DLSS_on12:
        return UpscalerDisplayName(Upscaler::DLSS);
    default:
        return UpscalerDisplayName(upscaler, api);
    }
}

static feature_version UpscalerSortVersion(Upscaler upscaler)
{
    switch (upscaler)
    {
    case Upscaler::FSR21:
    case Upscaler::FSR21_on12:
        return { 2, 1, 2 };
    case Upscaler::FSR22:
    case Upscaler::FSR22_on12:
        return { 2, 2, 1 };
    case Upscaler::FSR31:
        return { 3, 1, 2 };
    case Upscaler::FFX:
    case Upscaler::FFX_on12:
        return { 3, 1, 0 };
    default:
        return {};
    }
}

// Indices into the FFX version list, newest first
static std::vector<int> FfxVersionsNewestFirst(const State::FfxVersionList& versions)
{
    std::vector<int> order(versions.names.size());
    std::iota(order.begin(), order.end(), 0);

    std::stable_sort(order.begin(), order.end(), [&versions](int a, int b)
                     { return feature_version(versions.names[b]) < feature_version(versions.names[a]); });

    return order;
}

void MenuCommon::RenderUpscalerCombo(const API api, Upscaler currentUpscaler, const std::vector<Upscaler>& options)
{
    auto primaryGpu = IdentifyGpu::getPrimaryGpu();
    auto config = Config::Instance();
    const int ffxIndex = config->FfxUpscalerIndex.value_or_default();

    Upscaler targetBackend = State::Instance().newBackend;
    if (targetBackend == Upscaler::Reset)
        targetBackend = currentUpscaler;

    // FSR names already include the version, others only know theirs once running
    auto upscalerName = [&](Upscaler upscaler, int index)
    {
        auto feature = State::Instance().currentFeature;

        if (upscaler == currentUpscaler && !IsFsr(upscaler) && feature != nullptr)
            return NameWithVersion(feature);

        return ShortUpscalerName(upscaler, api, index);
    };

    const auto currentCustom = CustomProviders::Selected(api, currentUpscaler);

    // Interop is only shown here, the list uses a tag for it
    auto selectedName = upscalerName(targetBackend, ffxIndex);
    if (auto custom = CustomProviders::Selected(api, targetBackend); !custom.empty())
        selectedName = custom;
    else if (IsOn12(targetBackend))
        selectedName += " (Dx12)";

    if (!ImGui::BeginCombo("##UpscalerCombo", selectedName.c_str(), ImGuiComboFlags_HeightLargest))
        return;

    struct Entry
    {
        Upscaler upscaler;
        int ffxIndex; // Newest upscaler reported by FFX, -1 when not using FFX
        feature_version version;
        std::string custom; // Custom FFX or DLSS upscaler name
    };

    std::vector<Entry> entries;

    for (auto opt : options)
    {
        // Check if GPU is capable of a given backend
        if ((opt == Upscaler::DLSS || opt == Upscaler::DLSS_on12) && !primaryGpu.dlssCapable)
            continue;

        // Not all Intel GPUs support native DX11 XeSS but don't think we have a good way to check exactly
        if (opt == Upscaler::XeSS && api == API::DX11 && primaryGpu.vendorId != VendorId::Intel)
            continue;

        // Only the newest FFX upscaler is listed here, the rest are in the FFX Upscaler combo
        if (auto versions = GetFfxVersionList(opt, api); versions != nullptr)
        {
            if (versions == &State::Instance().ffxUpscalerVersionsVk)
                FFXFeature::EnsureVersionsVulkan();
            else
                FFXFeature::EnsureVersionsDx12();

            // Fall back to a generic entry if FFX couldn't be queried
            if (!versions->names.empty())
            {
                // Labelled with the remembered upscaler, sorted by the newest one so picking another FFX version
                // doesn't move the entry
                auto newest = FfxVersionsNewestFirst(*versions)[0];
                auto index = ffxIndex >= 0 && ffxIndex < versions->names.size() ? ffxIndex : newest;
                entries.push_back({ opt, index, feature_version(versions->names[newest]) });
                continue;
            }
        }

        entries.push_back({ opt, -1, UpscalerSortVersion(opt) });
    }

    // Custom ones are sorted after the regular ones, their names don't need versions
    // DLSS ones don't need a DLSS capable GPU
    std::vector<std::string> customNames;

    for (auto kind : { CustomProviders::Kind::Ffx, CustomProviders::Kind::FfxVk, CustomProviders::Kind::Dlss })
    {
        for (auto& name : CustomProviders::List(kind))
        {
            if (!CustomProviders::Contains(customNames, name))
                customNames.push_back(name);
        }
    }

    for (auto& name : customNames)
    {
        for (auto upscaler : CustomProviders::Upscalers(name, api))
        {
            const bool ffx = upscaler == Upscaler::FFX || upscaler == Upscaler::FFX_on12;
            entries.push_back({ upscaler, ffx ? config->FfxUpscalerIndexFor(name).value_or_default() : -1, {}, name });
        }
    }

    // Upscaler of the GPU vendor goes first, FSRs are sorted by version, newest first
    // Stable sort keeps native ahead of w/Dx12
    auto group = [&primaryGpu](Upscaler upscaler)
    {
        int family = IsFsr(upscaler) ? 1 : (upscaler == Upscaler::DLSS || upscaler == Upscaler::DLSS_on12) ? 2 : 0;

        const int vendorFamily = primaryGpu.vendorId == VendorId::AMD      ? 1
                                 : primaryGpu.vendorId == VendorId::Nvidia ? 2
                                                                           : 0;

        return family == vendorFamily ? -1 : family;
    };

    std::stable_sort(entries.begin(), entries.end(),
                     [&group](const Entry& a, const Entry& b)
                     {
                         if (group(a.upscaler) != group(b.upscaler))
                             return group(a.upscaler) < group(b.upscaler);

                         return b.version < a.version;
                     });

    for (size_t i = 0; i < entries.size(); i++)
    {
        const auto& entry = entries[i];

        if (i > 0 && group(entry.upscaler) != group(entries[i - 1].upscaler))
            ImGui::Separator();

        auto name = entry.custom.empty() ? upscalerName(entry.upscaler, entry.ffxIndex) : entry.custom;
        auto label = StrFmt("%s##%d%s", name.c_str(), (int) entry.upscaler, entry.custom.c_str());
        // Folder names in Dx12Upscaler aren't case sensitive
        const bool selected =
            currentUpscaler == entry.upscaler && _stricmp(currentCustom.c_str(), entry.custom.c_str()) == 0;

        if (ImGui::Selectable(label.c_str(), selected))
        {
            State::Instance().newBackend = entry.upscaler;

            // Regular and custom entries share an upscaler type, which the backend change check compares,
            // so a switch between them is marked here
            if (auto& customSetting = CustomProviders::NameSetting(api);
                !selected && entry.custom != customSetting.value_or("") &&
                (!entry.custom.empty() || CustomProviders::Runs(customSetting.value_or(""), api, entry.upscaler)))
            {
                if (entry.custom.empty())
                    customSetting.reset();
                else
                    customSetting = entry.custom;

                MARK_ALL_BACKENDS_CHANGED();
            }
        }

        // Dimmed tags aligned to the right
        const char* tag = nullptr;

        if (entry.ffxIndex >= 0)
            tag = IsOn12(entry.upscaler) ? "FFX Dx12" : "FFX";
        else if (!entry.custom.empty())
            tag = IsOn12(entry.upscaler) ? "DLSS Dx12" : "DLSS";
        else if (IsOn12(entry.upscaler))
            tag = "Dx12";

        if (tag != nullptr)
        {
            auto& style = ImGui::GetStyle();

            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - style.WindowPadding.x -
                                                                      ImGui::CalcTextSize(tag).x));
            ImGui::TextDisabled(tag);
        }
    }

    ImGui::EndCombo();
}

void MenuCommon::AddDx11Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX11, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR22, Upscaler::FSR31, Upscaler::XeSS_on12, Upscaler::FSR21_on12,
                          Upscaler::FSR22_on12, Upscaler::FFX_on12, Upscaler::DLSS, Upscaler::DLSS_on12 });
}

void MenuCommon::AddDx12Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX12, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::DLSS });
}

void MenuCommon::AddVulkanBackends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::Vulkan, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::FSR21_on12,
                          Upscaler::FFX_on12, Upscaler::DLSS, Upscaler::DLSS_on12 });
}

// Upscaler for the game's ray reconstruction: DLSSDs first, NVIDIA's then custom ones, regular upscalers after.
// Feature is the running one, nullptr before the game starts ray reconstruction
static void RenderRRUpscalerCombo(API api, IFeature* feature, const char* label = "##RRUpscalerCombo")
{
    auto& state = State::Instance();
    auto config = Config::Instance();

    // Empty for NVIDIA's, custom ones until they turn out not to run on the api
    std::vector<std::string> entries;

    if (Nvngx_RR::NativeAvailable())
        entries.push_back({});

    for (auto& name : CustomProviders::List(CustomProviders::Kind::Dlssd))
    {
        if (Nvngx_RR::Listable(name, api))
            entries.push_back(name);
    }

    auto entryName = [](const std::string& entry) { return entry.empty() ? std::string("DLSSD") : entry; };

    // Regular upscalers take ray reconstruction without denoising it, when allowed
    const bool allowRegulars = config->RRAllowRegularUpscalers.value_or_default();
    std::vector<Upscaler> regulars;

    if (allowRegulars)
    {
        regulars = { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX };

        if (IdentifyGpu::getPrimaryGpu().dlssCapable)
            regulars.push_back(Upscaler::DLSS);
    }

    const auto picked = Nvngx_RR::Regular();

    // Running: the FSR fallback isn't one of them. Not running yet: the configured one, a DLSSD unless a regular
    // upscaler is picked, Off when no DLSSD can run
    auto* dlssd = dynamic_cast<DLSSDFeature*>(feature);
    const bool runsDlssd = feature != nullptr ? dlssd != nullptr : !picked.has_value() && !entries.empty();
    const auto current = dlssd != nullptr ? dlssd->CustomName() : config->CustomRRUpscaler.value_or("");

    std::string preview;

    if (runsDlssd)
        preview = entryName(current);
    else if (feature != nullptr)
        preview = NameWithVersion(feature);
    else if (picked.has_value())
        preview = UpscalerDisplayName(picked.value(), api);
    else
        preview = "关";

    if (!ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLargest))
        return;

    auto changeAll = [&state]()
    {
        Nvngx_RR::ResetFailures();

        for (auto handle : state.rrHandles)
            state.changeBackend[handle] = true;
    };

    for (const auto& entry : entries)
    {
        // Folder names in RRUpscaler aren't case sensitive
        const bool selected = runsDlssd && _stricmp(current.c_str(), entry.c_str()) == 0;
        auto label = StrFmt("%s##rr%s", entryName(entry).c_str(), entry.c_str());

        if (ImGui::Selectable(label.c_str(), selected) && !selected)
        {
            // NVIDIA's is auto, it's picked when the gpu can run it
            if (entry.empty())
                config->CustomRRUpscaler.reset();
            else
                config->CustomRRUpscaler = entry;

            config->RRRegularUpscaler.reset();
            changeAll();
        }
    }

    // Without a DLSSD ray reconstruction is only offered to the game for a regular upscaler
    if (allowRegulars && entries.empty())
    {
        if (ImGui::Selectable("关##rrOff", !picked.has_value()))
            config->RRRegularUpscaler.reset();
    }

    if (allowRegulars)
        ImGui::Separator();

    for (auto upscaler : regulars)
    {
        const bool selected = !runsDlssd && picked == upscaler;
        auto label = StrFmt("%s##rrRegular%d", UpscalerDisplayName(upscaler, api).c_str(), (int) upscaler);

        if (ImGui::Selectable(label.c_str(), selected) && !selected)
        {
            config->RRRegularUpscaler = upscaler;
            changeAll();
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() -
                                                                  ImGui::GetStyle().WindowPadding.x -
                                                                  ImGui::CalcTextSize("不降噪").x));
        ImGui::TextDisabled("不降噪");
    }

    ImGui::EndCombo();
}

static ImVec4 StatusWarn();

// Ray reconstruction's upscaler before the game starts it. Without a DLSSD the game checks at start whether it's
// offered, so changing that needs a restart
static void RenderRROfferCombo(API api)
{
    const auto picked = Nvngx_RR::Regular();
    static const bool pickedAtStart = picked.has_value();
    const bool dlssdAvailable = Nvngx_RR::DlssdAvailable();

    RenderRRUpscalerCombo(api, nullptr, "光线重建##RROfferCombo");

    if (ImGui::IsItemHovered())
    {
        if (dlssdAvailable)
            ImGui::SetTooltip("游戏中开启光线重建时使用的超分器。");
        else
            ImGui::SetTooltip("此 GPU 无法运行 DLSSD，因此游戏不会提供光线重建。\n"
                              "在此选择超分器后，即可在游戏中开启光线重建。");
    }

    if (picked.has_value())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped("此超分器无法降噪。\n游戏开启 DLSSD 时，画面可能出现颗粒或噪点。");
        ImGui::PopStyleColor();
    }

    if (!dlssdAvailable && picked.has_value() != pickedAtStart)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped("保存设置并重启游戏以应用");
        ImGui::PopStyleColor();
    }
}

template <HasDefaultValue B> void MenuCommon::AddResourceBarrier(std::string name, CustomOptional<int32_t, B>* value)
{
    const char* states[] = { "AUTO",
                             "COMMON",
                             "VERTEX_AND_CONSTANT_BUFFER",
                             "INDEX_BUFFER",
                             "RENDER_TARGET",
                             "UNORDERED_ACCESS",
                             "DEPTH_WRITE",
                             "DEPTH_READ",
                             "NON_PIXEL_SHADER_RESOURCE",
                             "PIXEL_SHADER_RESOURCE",
                             "STREAM_OUT",
                             "INDIRECT_ARGUMENT",
                             "COPY_DEST",
                             "COPY_SOURCE",
                             "RESOLVE_DEST",
                             "RESOLVE_SOURCE",
                             "RAYTRACING_ACCELERATION_STRUCTURE",
                             "SHADING_RATE_SOURCE",
                             "GENERIC_READ",
                             "ALL_SHADER_RESOURCE",
                             "PRESENT",
                             "PREDICATION",
                             "VIDEO_DECODE_READ",
                             "VIDEO_DECODE_WRITE",
                             "VIDEO_PROCESS_READ",
                             "VIDEO_PROCESS_WRITE",
                             "VIDEO_ENCODE_READ",
                             "VIDEO_ENCODE_WRITE" };
    const int values[] = { -1,  0,   1,     2,      4,      8,      16,      32,       64,   128,
                           256, 512, 1024,  2048,   4096,   8192,   4194304, 16777216, 2755, 192,
                           0,   310, 65536, 131072, 262144, 524288, 2097152, 8388608 };

    int selected = value->value_or(-1);

    const char* selectedName = "";

    for (int n = 0; n < 28; n++)
    {
        if (values[n] == selected)
        {
            selectedName = states[n];
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), selectedName))
    {
        if (ImGui::Selectable(states[0], !value->has_value()))
            value->reset();

        for (int n = 1; n < 28; n++)
        {
            if (ImGui::Selectable(states[n], selected == values[n]))
                *value = values[n];
        }

        ImGui::EndCombo();
    }
}

static uint32_t GetPresetIndex(IFeature* feature, bool dlssd = false)
{
    auto ratio = (float) feature->TargetWidth() / (float) feature->RenderWidth();

    if (!dlssd)
    {
        if (State::Instance().dlssPresetsOverridenByOpti)
        {
            LOG_DEBUG("DLSS 预设已被 Opti 覆盖，使用 Opti 预设索引，倍率：{}", ratio);

            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssPresetsOverriddenExternally)
        {
            LOG_DEBUG("DLSS 预设已被外部覆盖，使用外部预设索引：{}",
                      State::Instance().dlssRenderPresetExternal);

            return State::Instance().dlssRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssRenderPresetDLAA;
            }
        }
    }
    else
    {
        if (State::Instance().dlssdPresetsOverridenByOpti)
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssdPresetsOverriddenExternally)
        {
            return State::Instance().dlssdRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssdRenderPresetDLAA;
            }
        }
    }

    return 0;
}

// TODO: disable presets based on the detected DLSS version
template <HasDefaultValue B> void MenuCommon::AddDLSSRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // clang-format off
    static const std::vector<MenuOption<uint32_t>> presets = {
        { NVSDK_NGX_DLSS_Hint_Render_Preset_Default, "默认",
            "跟随游戏设置" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_A, "PRESET A",
            "适用于性能/均衡/质量模式。\n较旧的变体，最适合对抗拖影……\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_B, "PRESET B",
            "适用于极致性能模式。\n与预设 A 类似……\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_C, "PRESET C",
            "适用于性能/均衡/质量模式。\n通常偏向当前帧信息……\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_D, "PRESET D",
            "性能/均衡/质量模式的默认预设；\n通常偏向图像稳定性。\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_E, "PRESET E",
            "DLSS 3.7+，更好的 D 预设\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_F, "PRESET F",
            "极致性能与 DLAA 模式的默认预设\n在近期版本中已移除！" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_G, "PRESET G",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_H_Reserved, "PRESET H",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_I_Reserved, "PRESET I",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_J, "PRESET J",
            "与预设 K 类似。预设 J 可能略微\n减少拖影……\n第 1 代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_K, "PRESET K",
            "DLAA/均衡/质量模式的默认预设……\n第 1 代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_L, "PRESET L",
            "极致性能模式的默认预设\n第 2 代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_M, "PRESET M",
            "性能模式的默认预设\n第 2 代 Transformer" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_N, "PRESET N",
            "未使用" },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_O, "PRESET O",
            "未使用" },
        { NV_PRESET_LATEST, "最新",
            "dll 支持的最新版本" }
    };
    // clang-format on

    PopulateCombo(name, *value, presets);
}

template <HasDefaultValue B> void MenuCommon::AddDLSSDRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // We don't have DLSSD definitions so using raw values
    static const std::vector<MenuOption<uint32_t>> presets = {
        { 0, "默认", "跟随游戏设置" },
        { 1, "PRESET A", "预设 A\n在近期版本中已移除！" },
        { 2, "PRESET B", "预设 B\n在近期版本中已移除！" },
        { 3, "PRESET C", "预设 C\n在近期版本中已移除！" },
        { 4, "PRESET D", "默认模型，Transformer" },
        { 5, "PRESET E", "最新 Transformer 模型\n如需景深引导则必须使用" },
        { 6, "PRESET F", "最新 Transformer 模型\n如需景深引导则必须使用" },
        { NV_PRESET_LATEST, "最新", "dll 支持的最新版本" }
    };

    PopulateCombo(name, *value, presets);
}

template <typename TStorage, typename T>
void MenuCommon::PopulateCombo(const std::string& name, TStorage& currentValue,
                               const std::vector<MenuOption<T>>& options)
{
    if (options.empty())
        return;

    // Assumes that different types mean that TStorage is std::optional
    T currentVal;
    if constexpr (std::is_same_v<TStorage, T>)
        currentVal = currentValue;
    else
        currentVal = currentValue.value_or(options[0].value);

    // Find the label for the currently selected item
    std::string preview = "未知";
    for (const auto& opt : options)
    {
        if (opt.value == currentVal)
        {
            preview = opt.label;
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), preview.c_str()))
    {
        for (const auto& opt : options)
        {
            if (opt.hidden)
                continue;

            if (opt.disabled)
                ImGui::BeginDisabled();

            bool isSelected = (currentVal == opt.value);
            if (ImGui::Selectable(opt.label.c_str(), isSelected))
                currentValue = opt.value;

            // Show tooltip for the individual item if it exists
            if (!opt.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", opt.tooltip.c_str());

            if (opt.disabled)
                ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
}

static UiTargetMode getUiTargetMode()
{
    const auto& state = State::Instance();

    const bool fallback = !Config::Instance()->OverlayMenu.value_or_default();

    if (fallback)
    {
        // We have no reliable swapchain/output color-space information here.
        // Only classify the upscaled working image.
        if (state.currentFeature && state.currentFeature->IsHdr())
            return UiTargetMode::LinearHDR;

        return UiTargetMode::SDR;
    }

    // sRGB target encodes on write, colors need to be linear
    if (state.menuTargetSrgb)
        return UiTargetMode::LinearHDR;

    const auto& output = state.outputColorSpace;

    // If SetColorSpace1 has not provided a known/valid color space,
    // fall back conservatively.
    if (!output.valid)
        return UiTargetMode::SDR;

    switch (output.transfer)
    {
    case ColorTransfer::Linear:
        // scRGB: linear Rec.709 RGB.
        //
        // hdrOutputActive is intentionally NOT required here.
        // A scRGB swapchain is still linear even when the physical output
        // is currently SDR. hdrOutputActive only affects the desired
        // reference-white scaling in toneMapColor().
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec709)
        {
            return UiTargetMode::ScRGB;
        }

        break;

    case ColorTransfer::PQ:
        // Direct PQ UI rendering currently assumes RGB PQ / Rec.2020.
        //
        // Do not treat YCbCr PQ as an RGB render target.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::PQ;
        }

        break;

    case ColorTransfer::HLG:
        // Current HLG UI path assumes an RGB render target.
        //
        // DXGI HLG modes are commonly YCbCr, so reject unsupported
        // combinations rather than applying an RGB HLG transform blindly.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::HLG;
        }

        break;

    case ColorTransfer::SRGB:
        return UiTargetMode::SDR;

    case ColorTransfer::Unknown:
    default:
        break;
    }

    // Unsupported model / primaries / transfer combination.
    return UiTargetMode::SDR;
}

static float srgbToLinear(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);

    if (x <= 0.04045f)
        return x / 12.92f;

    return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

static float linearToPQ(float nits)
{
    // SMPTE ST.2084
    constexpr float m1 = 2610.0f / 16384.0f;
    constexpr float m2 = 2523.0f / 32.0f;
    constexpr float c1 = 3424.0f / 4096.0f;
    constexpr float c2 = 2413.0f / 128.0f;
    constexpr float c3 = 2392.0f / 128.0f;

    const float y = std::clamp(nits / 10000.0f, 0.0f, 1.0f);
    const float ym1 = std::pow(y, m1);

    return std::pow((c1 + c2 * ym1) / (1.0f + c3 * ym1), m2);
}

static float linearToHLG(float x)
{
    // BT.2100 HLG OETF
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;

    x = std::max(x, 0.0f);

    if (x <= (1.0f / 12.0f))
        return std::sqrt(3.0f * x);

    return a * std::log(12.0f * x - b) + c;
}

static ImVec4 linear709To2020(float r, float g, float b)
{
    return ImVec4(0.6274040f * r + 0.3292820f * g + 0.0433136f * b, 0.0690970f * r + 0.9195400f * g + 0.0113612f * b,
                  0.0163916f * r + 0.0880132f * g + 0.8955950f * b, 0.0f);
}

static ImVec4 legacyHdrToneMap(const ImVec4& color)
{
    constexpr float exposure = 1.0f;
    constexpr float strength = 1.0f;

    const float peak = std::max(color.x, std::max(color.y, color.z));

    if (peak <= 0.0f)
        return color;

    const float exposedPeak = peak * exposure;
    const float mappedPeak = exposedPeak / (1.0f + exposedPeak);

    const float reinhardScale = mappedPeak / peak;
    const float scale = 1.0f + (reinhardScale - 1.0f) * strength;

    return ImVec4(color.x * scale, color.y * scale, color.z * scale, color.w);
}

static ImVec4 toneMapColor(const ImVec4& color);

// Status colors shared by the whole menu
static ImVec4 StatusOk() { return toneMapColor(ImVec4(0.35f, 0.85f, 0.45f, 1.f)); }
static ImVec4 StatusWarn() { return toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)); }
static ImVec4 StatusError() { return toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)); }

static ImVec4 toneMapColor(const ImVec4& color)
{
    const auto mode = getUiTargetMode();

    switch (mode)
    {
    case UiTargetMode::SDR:
        return color;

    case UiTargetMode::LinearHDR:
        return ImVec4(srgbToLinear(color.x), srgbToLinear(color.y), srgbToLinear(color.z), color.w);

    case UiTargetMode::ScRGB:
    {
        constexpr float scRgbReferenceWhiteNits = 80.0f;
        constexpr float hdrUiWhiteNits = 203.0f;

        const float uiWhiteNits = State::Instance().hdrOutputActive ? hdrUiWhiteNits : scRgbReferenceWhiteNits;

        const float scale = uiWhiteNits / scRgbReferenceWhiteNits;

        return ImVec4(srgbToLinear(color.x) * scale, srgbToLinear(color.y) * scale, srgbToLinear(color.z) * scale,
                      color.w);
    }

    case UiTargetMode::PQ:
        // Direct ImGui rendering into a nonlinear PQ target.
        //
        // Proper PQ encoding of vertex colors produces incorrect results
        // with the standard ImGui alpha blend state because blending then
        // happens in PQ space.
        //
        // Keep the known-good legacy compression until PQ rendering is
        // moved to a linear intermediate/composite pass.
        return legacyHdrToneMap(color);

    case UiTargetMode::HLG:
        // Same fundamental nonlinear-blending problem as PQ.
        // Conservative compatibility behavior for now.
        return legacyHdrToneMap(color);

    default:
        return color;
    }
}

static void MenuHdrCheck(ImGuiIO io)
{
    if (!_hdrTonemapApplied)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const auto mode = getUiTargetMode();

        LOG_INFO("输出 HDR：{}，UI 模式：{}", State::Instance().hdrOutputActive, magic_enum::enum_name(mode));

        CopyMemory(SdrColors, style.Colors, sizeof(style.Colors));

        // Apply tone mapping to the ImGui style
        for (int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            ImVec4 color = style.Colors[i];
            style.Colors[i] = toneMapColor(color);
        }

        _hdrTonemapApplied = true;
    }
}

static float MenuResolutionScale(ImGuiIO io)
{
    if (Config::Instance()->MenuScale.has_value())
        return Config::Instance()->MenuScale.value();

    // Calculate menu scale according to display resolution
    float y = State::Instance().screenHeight;

    if (io.DisplaySize.y != 0)
        y = (float) io.DisplaySize.y;

    // 1000p is minimum for 1.0 menu ratio
    float result = (float) ((int) (y / 108.0f)) / 10.0f;

    result = std::round(result * 10.0f) / 10.0f;

    if (result < 0.5f)
        result = 0.5f;

    if (result > 2.0f)
        result = 2.0f;

    return result;
}

inline static std::string GetSourceString(UINT source)
{
    switch (source)
    {
    case 1:
        return "RTV";
    case 2:
        return "SRV";
    case 4:
        return "UAV";
    case 8:
        return "OM";
    case 16:
        return "Ups";
    case 32:
        return "SCR";
    case 64:
        return "SGR";
    case 128:
        return "OMUAV";
    default:
        return std::format("{}", source);
    }
}

inline static std::string GetDispatchString(UINT source)
{
    switch (source)
    {
    case 0:
        return "-";
    case 512:
        return "DI";
    case 1024:
        return "DII";
    case 256:
        return "Disp";
    default:
        return std::format("{}", source);
    }
}

void MenuCommon::ApplyThemeStyle()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    auto conf = Config::Instance();
    bool lightTheme = conf->LightTheme.value_or_default();

    style.WindowRounding = conf->MenuRounding.value_or_default();
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.FrameBorderSize = lightTheme ? 1.0f : 0.0f;
    style.TabBorderSize = lightTheme ? 1.0f : 0.0f;

    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    auto Clamp01 = [](float v) { return std::max(0.0f, std::min(v, 1.0f)); };

    auto Mix = [](const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
    { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };

    auto Luminance = [](const ImVec4& c) { return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f; };

    auto Saturate = [&](const ImVec4& color, float amount)
    {
        float lum = Luminance(color);

        return ImVec4(Clamp01(lum + (color.x - lum) * amount), Clamp01(lum + (color.y - lum) * amount),
                      Clamp01(lum + (color.z - lum) * amount), color.w);
    };

    ImVec4 accent = ImVec4(conf->MenuAccentColorR.value_or_default(), conf->MenuAccentColorG.value_or_default(),
                           conf->MenuAccentColorB.value_or_default(), 1.0f);

    ImVec4 bgAccent = ImVec4(conf->MenuBGColorR.value_or_default(), conf->MenuBGColorG.value_or_default(),
                             conf->MenuBGColorB.value_or_default(), 1.0f);

    float luminance = Luminance(accent);

    const ImVec4 bgDark = lightTheme ? ImVec4(0.80f, 0.82f, 0.86f, 1.00f) : ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    const ImVec4 bgMid = lightTheme ? ImVec4(0.89f, 0.91f, 0.95f, 1.00f) : ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    const ImVec4 bgLight = lightTheme ? ImVec4(0.96f, 0.97f, 0.99f, 1.00f) : ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

    const ImVec4 textPrimary = lightTheme ? ImVec4(0.05f, 0.06f, 0.08f, 1.00f) : ImVec4(0.90f, 0.93f, 0.95f, 1.00f);
    const ImVec4 textDim = lightTheme ? ImVec4(0.22f, 0.25f, 0.31f, 1.00f) : ImVec4(0.54f, 0.58f, 0.62f, 1.00f);

    const ImVec4 borderCol = lightTheme ? ImVec4(0.35f, 0.40f, 0.50f, 1.00f) : ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    const ImVec4 dimBg = lightTheme ? ImVec4(0.30f, 0.33f, 0.38f, 0.20f) : ImVec4(0.09f, 0.10f, 0.13f, 0.20f);
    const ImVec4 modalDimBg = lightTheme ? ImVec4(0.22f, 0.24f, 0.28f, 0.55f) : ImVec4(0.04f, 0.04f, 0.07f, 0.55f);

    // MenuBGColor: only background/surface tint.
    auto BgTint = [&](const ImVec4& base, float strength = 1.0f, float alpha = 1.0f)
    {
        float t = lightTheme ? (0.180f * strength) : (0.120f * strength);
        return Mix(base, bgAccent, t, alpha);
    };

    // MenuAccentColor: all visible interactive accent colors.
    auto AccentSoft = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.14f, alpha) : Mix(bgDark, accent, 0.32f, alpha); };

    auto AccentMed = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.42f, alpha) : Mix(bgDark, accent, 0.55f, alpha); };

    auto AccentStrong = [&](float alpha = 1.0f) { return ImVec4(accent.x, accent.y, accent.z, alpha); };

    const ImVec4 bgTitle = AccentSoft();

    auto SurfaceHover = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.12f, alpha) : Mix(bgLight, accent, 0.18f, alpha); };

    auto SurfaceActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.20f, alpha) : Mix(bgLight, accent, 0.28f, alpha); };

    auto TitleActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgTitle, accent, 0.18f, alpha) : Mix(bgTitle, accent, 0.16f, alpha); };

    auto PlotAccent = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            // Darken slightly for contrast on light bg — no channel floors
            return Mix(accent, ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.20f, alpha);
        }

        // Brighten slightly for visibility on dark bg — no channel floors
        return Mix(accent, ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.35f, alpha);
    };

    auto PlotAccentHovered = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            return Mix(PlotAccent(alpha), ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.15f, alpha);
        }

        return Mix(PlotAccent(alpha), ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.25f, alpha);
    };

    auto AccentReadable = [&](float alpha = 1.0f)
    {
        // Apply saturation boost and luminance correction only here,
        // so AccentStrong / AccentMed / AccentSoft stay true to the user's pick.
        ImVec4 a = Saturate(accent, lightTheme ? 1.35f : 1.25f);
        float lum = Luminance(a);

        if (lightTheme && lum > 0.72f)
            a = Mix(a, ImVec4(0.0f, 0.0f, 0.0f, 1.0f), 0.35f, 1.0f);

        if (!lightTheme && lum < 0.25f)
            a = Mix(a, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.30f, 1.0f);

        return ImVec4(a.x, a.y, a.z, alpha);
    };

    ImVec4* c = ImGui::GetStyle().Colors;

    float minAlpha = Config::Instance()->MenuBGColorA.value_or_default() >= 0.5f
                         ? Config::Instance()->MenuBGColorA.value_or_default()
                         : 0.5f;

    c[ImGuiCol_Text] = textPrimary;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_TextLink] = AccentReadable();

    // MenuBGColor only.
    c[ImGuiCol_WindowBg] = BgTint(bgDark, 1.00f, Config::Instance()->MenuBGColorA.value_or_default());
    c[ImGuiCol_ChildBg] = BgTint(bgMid, 1.10f, minAlpha + 0.1f);
    c[ImGuiCol_PopupBg] =
        lightTheme ? BgTint(bgLight, 0.90f) : BgTint(ImVec4(0.09f, 0.10f, 0.13f, 0.97f), 0.90f, 0.97f);
    c[ImGuiCol_MenuBarBg] = BgTint(bgDark, 0.85f);
    c[ImGuiCol_DockingEmptyBg] = BgTint(bgDark, 0.75f);

    c[ImGuiCol_Border] = borderCol;
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Neutral background, not MenuBGColor.
    c[ImGuiCol_FrameBg] = BgTint(bgLight, 0.50f, minAlpha + 0.15f);
    c[ImGuiCol_FrameBgHovered] = SurfaceHover();
    c[ImGuiCol_FrameBgActive] = SurfaceActive();

    c[ImGuiCol_TitleBg] = BgTint(bgTitle, 0.40f);
    c[ImGuiCol_TitleBgActive] = TitleActive();
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(bgTitle.x, bgTitle.y, bgTitle.z, 0.75f);

    c[ImGuiCol_ScrollbarBg] = BgTint(bgDark, 0.60f, minAlpha + 0.2f);
    c[ImGuiCol_ScrollbarGrab] = AccentSoft();
    c[ImGuiCol_ScrollbarGrabHovered] = AccentMed();
    c[ImGuiCol_ScrollbarGrabActive] = AccentStrong();

    c[ImGuiCol_CheckMark] = AccentReadable();
    c[ImGuiCol_SliderGrab] = AccentMed();
    c[ImGuiCol_SliderGrabActive] = AccentReadable();
    c[ImGuiCol_InputTextCursor] = AccentReadable();

    c[ImGuiCol_Button] = AccentSoft();
    c[ImGuiCol_ButtonHovered] = AccentMed();
    c[ImGuiCol_ButtonActive] = AccentStrong();

    c[ImGuiCol_Header] = AccentSoft(0.90f);
    c[ImGuiCol_HeaderHovered] = AccentMed(0.95f);
    c[ImGuiCol_HeaderActive] = AccentStrong();

    c[ImGuiCol_Separator] = borderCol;
    c[ImGuiCol_SeparatorHovered] = AccentMed(0.85f);
    c[ImGuiCol_SeparatorActive] = AccentStrong();

    c[ImGuiCol_ResizeGrip] = AccentSoft(0.30f);
    c[ImGuiCol_ResizeGripHovered] = AccentStrong(0.70f);
    c[ImGuiCol_ResizeGripActive] = AccentStrong(0.95f);

    c[ImGuiCol_Tab] = BgTint(bgDark, 0.60f);
    c[ImGuiCol_TabHovered] = AccentMed(0.85f);
    c[ImGuiCol_TabSelected] = AccentMed();
    c[ImGuiCol_TabSelectedOverline] = AccentStrong();
    c[ImGuiCol_TabDimmed] = BgTint(bgDark, 0.60f);
    c[ImGuiCol_TabDimmedSelected] = AccentSoft(0.75f);
    c[ImGuiCol_TabDimmedSelectedOverline] = borderCol;

    c[ImGuiCol_DockingPreview] = AccentStrong(0.70f);

    c[ImGuiCol_PlotLines] = PlotAccent();
    c[ImGuiCol_PlotLinesHovered] = PlotAccentHovered();
    c[ImGuiCol_PlotHistogram] = PlotAccent(0.85f);
    c[ImGuiCol_PlotHistogramHovered] = PlotAccentHovered();

    c[ImGuiCol_TableHeaderBg] = BgTint(bgMid, 0.80f, minAlpha + 0.25f);
    c[ImGuiCol_TableBorderStrong] = borderCol;
    c[ImGuiCol_TableBorderLight] = lightTheme ? ImVec4(0.68f, 0.72f, 0.80f, 1.00f) : AccentSoft();
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = lightTheme ? ImVec4(0.00f, 0.00f, 0.00f, 0.045f) : ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    c[ImGuiCol_TreeLines] = borderCol;
    c[ImGuiCol_TextSelectedBg] = AccentMed(0.38f);
    c[ImGuiCol_DragDropTarget] = AccentStrong(0.90f);
    c[ImGuiCol_NavCursor] = AccentReadable();
    c[ImGuiCol_NavWindowingHighlight] = AccentStrong(0.70f);
    c[ImGuiCol_NavWindowingDimBg] = dimBg;
    c[ImGuiCol_ModalWindowDimBg] = modalDimBg;

    _hdrTonemapApplied = false;
    MenuHdrCheck(ImGui::GetIO());
}

static double lastTime = 0.0;
static double lastFrameTime = 0.0;
static UINT64 uwpTargetFrame = 0;

void MenuCommon::Present()
{
    _frameCount++;

    auto now = Util::MillisecondsNow();

    if (lastTime > 0.0)
        lastFrameTime = now - lastTime;

    lastTime = now;

    if (_handle != nullptr)
        UpdateManualInput(_handle);
}

static PresentRateMeter fgOutputPresentRate;

// Streamline can present real and generated frames from different threads.
// Measure the actual output-present cadence, not the number of presents
// observed between two (independently paced) menu frames.
void MenuCommon::FGOutputPresent() { fgOutputPresentRate.Present(Util::MillisecondsNow()); }

void MenuCommon::FGOutputPresents(bool presents) { fgOutputPresentRate.SetEnabled(presents); }

struct VersionCheckStatus
{
    bool completed = false;
    bool updateAvailable = false;
    std::string latestTag;
    std::string latestUrl;
    std::string error;
};

struct MenuCommon::RenderMenuContext
{
    State& state;
    decltype(Config::Instance()) config;
    ImGuiIO& io;
    IFeature* currentFeature = nullptr;

    double now = 0.0;
    double frameTime = 0.0;
    double frameRate = 0.0;
    // Current Vulkan FG output rate, kept separate from the long-term history.
    double fgOutputFrameTimeMs = 0.0;
    float menuResScale = 1.0f;
    float fpsScale = 1.0f;
    float averageFrameTime = 0.0f;
    float averageUpscalerFT = 0.0f;

    bool frameTimesCalculated = false;
    bool newFrame = false;

    VersionCheckStatus versionStatus;
    std::string currentVersionText;

    // Cached when the menu is visible and shared by RenderMainMenuWindow section helpers.
    std::unique_ptr<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>> primaryGpu;
};

static std::string splashMessage;

void MenuCommon::UpdateRenderTiming(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;

    if (config->OverlayMenu.value_or_default())
    {
        _frameCount++;

        // FPS & frame time calculation
        if (lastTime > 0.0)
        {
            frameTime = now - lastTime;
            frameRate = 1000.0 / frameTime;
        }

        lastTime = now;

        if (_handle != nullptr)
            UpdateManualInput(_handle);
    }
    else
    {
        if (state.activeFgInput == FGInput::NoFG || state.activeFgOutput == FGOutput::NoFG)
            MenuCommon::Present();

        frameTime = lastFrameTime;
        frameRate = 1000.0 / frameTime;
    }

    ctx.fgOutputFrameTimeMs = fgOutputPresentRate.FrameTime(now);
    if (ctx.fgOutputFrameTimeMs > 0.0)
    {
        frameTime = ctx.fgOutputFrameTimeMs;
        frameRate = 1000.0 / frameTime;
    }

    state.frameTimes.pop_front();
    state.frameTimes.push_back(frameTime);
}

void MenuCommon::UpdateMenuInputMode(RenderMenuContext& ctx)
{
    auto& io = ctx.io;

    // Moved here to prevent gamepad key replay
    if (_isVisible)
    {
        if (hasGamepad)
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    }
    else
    {
        capturingKey = false;
        hasGamepad = (io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0;
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;
    }
}

void MenuCommon::HandleMenuShortcuts(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    // Handle Inputs
    {
        if (inputFG)
        {
            inputFG = false;

            // DLSSG via Nvngx - activeFgOutput is NoFG, game's DLSSG is controlled by Override DLSSG Ratio
            if (state.activeFgInput == FGInput::NvngxFG)
            {
                if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1)
                {
                    auto& ratio = config->FGDLSSGOverrideInterpolationCount;
                    if (ratio.has_value() && ratio.value() == 0)
                    {
                        // Off by the override, go back to what it was
                        if (fgHotkeySavedOverride > 0)
                            ratio = fgHotkeySavedOverride;
                        else
                            ratio.reset();
                        LOG_DEBUG("FG toggle key pressed, DLSSG ratio override back to {}",
                                  ratio.has_value() ? std::to_string(ratio.value()) : std::string("默认"));
                    }
                    else
                    {
                        fgHotkeySavedOverride = ratio.has_value() ? ratio.value() : -1;
                        ratio = 0;
                        LOG_DEBUG("FG toggle key pressed, turning DLSSG off with the ratio override");
                    }
                    StreamlineHooks::updateDlssgOptions();
                }
                else
                {
                    LOG_DEBUG("FG toggle key pressed, DLSSG ratio override isn't available yet");
                }
            }

            else if (state.activeFgInput != FGInput::NoFG && state.activeFgOutput != FGOutput::NoFG &&
                     (state.FGSwapchainExists() || state.activeFgInput == FGInput::NvngxFG))
            {
                if (fgHotkeyLeftDefault)
                {
                    // Second press goes back to following the game
                    fgHotkeyLeftDefault = false;
                    config->FGEnabled = std::optional<bool> {};
                    LOG_DEBUG("FG toggle key pressed, following the game again");
                }
                else if (config->FGFollowsGame())
                {
                    // Volatile so the ini keeps auto
                    fgHotkeyLeftDefault = true;
                    config->FGEnabled.set_volatile_value(!config->IsFGEnabled());
                    LOG_DEBUG("FG toggle key pressed, overriding the game with {}", config->FGEnabled.value());
                }
                else
                {
                    config->FGEnabled = !config->IsFGEnabled();
                    LOG_DEBUG("FG 切换键已按下，将 FGEnabled 设为 {}", config->FGEnabled.value());
                }

                if (config->IsFGEnabled())
                    state.fgChanged = true;
            }
        }

        if (inputFps)
        {
            inputFps = false;
            config->ShowFps = !config->ShowFps.value_or_default();
        }

        if (inputFpsCycle && config->ShowFps.value_or_default())
            config->FpsOverlayType = (FpsOverlay) ((config->FpsOverlayType.value_or_default() + 1) % FpsOverlay_COUNT);

        if (inputMenu)
        {
            inputMenu = false;
            _isVisible = !_isVisible;

            LOG_DEBUG("Menu key pressed, {0}", _isVisible ? "正在打开 ImGui" : "正在关闭 ImGui");

            if (_isVisible)
            {
                io.ClearEventsQueue();
                io.ClearInputKeys();
                io.ClearInputMouse();

                OptiInput::ResetMenuInputTransientState();

                ApplyThemeStyle();

                refreshRate = Util::GetActiveRefreshRate(_handle);

                auto optiPath = std::filesystem::path(Config::Instance()->MainDllPath.value());
                state.artursFgFileAvailable = enablerExists.Get(optiPath / L"dlss-enabler-headless.dll");
                state.nukemsFgFileAvailable = nukemsExists.Get(optiPath / L"dlssg_to_fsr3_amd_is_better.dll");

                if (State::Instance().currentFeature != nullptr)
                {
                    auto upscalerType = State::Instance().currentFeature->GetUpscalerType();

                    if (upscalerType == Upscaler::DLSSD)
                        comboPreset = config->DLSSDRenderPresetForAll.value_or_default();
                    else if (upscalerType == Upscaler::DLSS || upscalerType == Upscaler::DLSS_on12)
                        comboPreset = config->RenderPresetForAll.value_or_default();
                }
            }
            else
            {
                ImGui::CloseCurrentPopup();

                _showMipmapCalcWindow = false;
                _showHudlessWindow = false;
            }

            io.MouseDrawCursor = _isVisible;
            io.WantCaptureKeyboard = _isVisible;
            io.WantCaptureMouse = _isVisible;
        }

        inputFpsCycle = false;
    }
}

void MenuCommon::UpdateVersionAndStartupNotifications(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& versionStatus = ctx.versionStatus;

    constexpr double splashTime = 7000.0;
    constexpr int updateNoticeTime = 10000;

    // Version check state is copied while locked, then consumed by the UI render pass.
    {
        std::scoped_lock lock(state.versionCheckMutex);
        versionStatus.completed = state.versionCheckCompleted;
        versionStatus.updateAvailable = state.updateAvailable;
        versionStatus.latestTag = state.latestVersionTag;
        versionStatus.latestUrl = state.latestVersionUrl;
        versionStatus.error = state.versionCheckError;
    }

    ctx.currentVersionText = VersionCheck::CurrentVersionString();

    if (versionStatus.completed && versionStatus.updateAvailable && !versionStatus.latestTag.empty())
    {
        if (updateNoticeTag != versionStatus.latestTag)
        {
            updateNoticeTag = versionStatus.latestTag;
            updateNoticeUrl = versionStatus.latestUrl;
            const auto notice = [&]()
            {
                ImGuiToast updateNotification { ImGuiToastType::Error, updateNoticeTime };
                updateNotification.setTitle("发现 OptiScaler 更新");
                updateNotification.setContent(
                    "按 %s 查看更多信息",
                    Keybind::KeyNameFromVirtualKeyCode(config->ShortcutKey.value_or_default()).c_str());
                ImGui::InsertNotification(updateNotification);
                return true;
            };
            static auto res = notice();
        }
    }

    // One-shot startup warning notifications.
    if (!state.postDone)
    {
        if (state.postCodes & PostCode::SlPluginsAlreadyInMemory)
        {
            auto filename = Util::DllPath().filename().string();
            to_lower_in_place(filename);

            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle("检测到较晚的 Streamline 挂钩");
            notification.setContent(
                "请考虑将 OptiScaler 从 %s 重命名为其他受支持的名称。\n否则可能会遇到问题。",
                filename.c_str());
            ImGui::InsertNotification(notification);
        }

        if (state.postCodes & PostCode::TryingFsr4Fp8OnUnsupported)
        {
            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle("检测到小笨蛋");
            notification.setContent("FSR 4 FP8 仅在 AMD 上可用");
            ImGui::InsertNotification(notification);
        }

        state.postDone = true;
    }

    // Initialize splash timing and select the splash text once per process.
    if (splashLimit < 1.0f)
    {
        splashStart = now + 100.0;
        splashLimit = splashStart + splashTime;

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        splashMessage = splashText[std::rand() % splashText.size()];
    }
}

void MenuCommon::BeginMenuFrameIfNeeded(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;
    auto& newFrame = ctx.newFrame;

    // New frame check
    if ((!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit) ||
        config->ShowFps.value_or_default() || _isVisible || ImGui::notifications.size() > 0)
    {
        if (!_isUWP)
        {
            ImGui_ImplWin32_NewFrame();
        }
        else
        {
            ImVec2 displaySize { state.screenWidth, state.screenHeight };
            ImGui_ImplUwp_NewFrame(displaySize);
        }

        OptiInput::FeedImGui(_isVisible);

        MenuHdrCheck(io);
        UpdateOverlayFont();
        ImGui::NewFrame();

        newFrame = true;
    }
}

void MenuCommon::RenderSplashWindow(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;

    constexpr double fadeTime = 1000.0;

    // Splash screen
    if (!config->DisableSplash.value_or_default())
    {
        if (now > splashStart && now < splashLimit)
        {

            ImGui::SetNextWindowSize({ 0.0f, 0.0f });
            ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default());
            ImGui::SetNextWindowPos(splashPosition, ImGuiCond_Always);

            float windowAlpha = 1.0f;
            if (auto diff = now - splashStart; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);
            else if (auto diff = splashLimit - now; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

            if (!config->OverlaysUseTheme.value_or_default())
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            }

            if (ImGui::Begin("启动画面", nullptr,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav))
            {
                float splashScale = 1.0f;
                float baseScaleHeight = 720.0f;

                if (io.DisplaySize.y > baseScaleHeight)
                    splashScale = io.DisplaySize.y / baseScaleHeight;

                if (config->UseHQFont.value_or_default())
                    ImGui::PushFontSize(std::round(splashScale * fontSize));
                else
                    ImGui::SetWindowFontScale(splashScale);

                ImGui::Text("OptiScaler - %s 菜单",
                            Keybind::KeyNameFromVirtualKeyCode(config->ShortcutKey.value_or_default()).c_str());
                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 0.7f)), splashMessage.c_str());

                splashSize = ImGui::GetWindowSize();

                if (config->UseHQFont.value_or_default())
                    ImGui::PopFontSize();

                ImGui::End();

                splashPosition.x = 0.0f; // io.DisplaySize.x - splashWinSize.x;
                splashPosition.y = io.DisplaySize.y - splashSize.y;
            }

            if (!config->OverlaysUseTheme.value_or_default())
                ImGui::PopStyleColor(4);
            else
                ImGui::PopStyleColor(2);

            ImGui::PopStyleVar(2);
        }
    }
}

void MenuCommon::RenderNotifications(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;

    // Notifications
    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool tonemapRequired = uiTargetMode != UiTargetMode::SDR;

    float screenHeight = State::Instance().screenHeight;
    if (io.DisplaySize.y != 0)
        screenHeight = io.DisplaySize.y;

    // Map resolution height to scale, 0.5 for 480p, 2.0 for 1440p
    constexpr float slope = (2.0f - 0.5f) / (1440.f - 480.f);
    float notificationScale = 0.5f + slope * (screenHeight - 480.f);
    notificationScale = std::clamp(notificationScale, 0.5f, 2.0f);

    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(notificationScale * fontSize));

    // No fallback font, SetWindowFontScale needs to be called after Begin()

    ImGui::RenderNotifications(ImGuiToastPos::TopCenter, notificationScale, tonemapRequired);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void MenuCommon::UpdateFrameTimeAverages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // FPS Overlay font
    fpsScale = config->FpsScale.value_or(menuResScale);

    // Update frame time & upscaler time averages
    averageFrameTime = 0.0f;
    averageUpscalerFT = 0.0f;

    if (config->ShowFps.value_or_default() || _isVisible)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
        frameTimesCalculated = true;

        // Vulkan FG output timing is already averaged across the last 125 ms.
        // Use it directly for the current FPS readout; preserve the shared
        // history for the separately labelled average and frame-time graph.
        if (ctx.fgOutputFrameTimeMs > 0.0)
        {
            frameTime = ctx.fgOutputFrameTimeMs;
            frameRate = 1000.0 / frameTime;
        }

        float lastFT = static_cast<float>(state.frameTimes.empty() ? 0.0f : state.frameTimes.back());
        float lastUT = static_cast<float>(state.upscaleTimes.empty() ? 0.0f : state.upscaleTimes.back());

        if (!fgOutputPresentRate.TakeIntervals([](double ms) { gFrameTimes.Push(static_cast<float>(ms)); }))
            gFrameTimes.Push(lastFT);

        gUpscalerTimes.Push(lastUT);

        averageFrameTime = gFrameTimes.Average();
        averageUpscalerFT = gUpscalerTimes.Average();
    }
}

void MenuCommon::RenderPerformanceOverlay(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // If Fps overlay is visible
    if (config->ShowFps.value_or_default())
    {
        bool stylePushed = false;

        const static auto defaultStyle = ImGuiStyle();

        // Rescale the fps overlay every frame because it shares style with the main menu
        if (config->FpsScale.has_value() && config->FpsScale.value() != menuResScale)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, defaultStyle.FramePadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, defaultStyle.SeparatorTextPadding * fpsScale);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, defaultStyle.ItemInnerSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, defaultStyle.IndentSpacing * fpsScale);

            stylePushed = true;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, config->FpsOverlayRounding.value_or_default() * fpsScale);

        // Set overlay position
        ImGui::SetNextWindowPos(overlayPosition, ImGuiCond_Always);

        // Set overlay window properties
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));  // Transparent border
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0)); // Transparent frame background

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  toneMapColor(RgbToColor(config->FpsOverlayLabelColor.value_or_default())));
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  toneMapColor(RgbToColor(config->FpsOverlayBGColor.value_or_default())));
        }

        // The background is drawn below, so a corner can stay square
        ImGui::SetNextWindowBgAlpha(0.0f);

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImGui::PushStyleColor(ImGuiCol_PlotLines,
                                  toneMapColor(RgbToColor(config->FpsOverlayGraphColor.value_or_default())));
        }

        if (ImGui::Begin("性能覆盖层", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav))
        {
            // Rounded corners, except one that touches two edges of the screen
            {
                const ImVec2 min = ImGui::GetWindowPos();
                const ImVec2 max = min + ImGui::GetWindowSize();
                const bool left = min.x <= 0.5f;
                const bool right = max.x >= io.DisplaySize.x - 0.5f;
                const bool top = min.y <= 0.5f;
                const bool bottom = max.y >= io.DisplaySize.y - 0.5f;

                ImDrawFlags corners = 0;
                if (!(top && left))
                    corners |= ImDrawFlags_RoundCornersTopLeft;
                if (!(top && right))
                    corners |= ImDrawFlags_RoundCornersTopRight;
                if (!(bottom && left))
                    corners |= ImDrawFlags_RoundCornersBottomLeft;
                if (!(bottom && right))
                    corners |= ImDrawFlags_RoundCornersBottomRight;

                auto background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
                background.w = config->FpsOverlayAlpha.value_or_default() * ImGui::GetStyle().Alpha;

                // The whole window, its content is clipped inside the padding
                auto drawList = ImGui::GetWindowDrawList();
                drawList->PushClipRect(min, max, false);
                drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(background),
                                        ImGui::GetStyle().WindowRounding,
                                        corners != 0 ? corners : ImDrawFlags_RoundCornersNone);
                drawList->PopClipRect();
            }

            std::string api;
            if (IdentifyGpu::gameUsesDxvk())
            {
                if (state.swapchainInteropApi == SwapchainInteropApi::None)
                    api = "DXVK";
                else
                    api = "DXVK 搭配 Dx12";
            }
            else if (IdentifyGpu::gameUsesVkd3dProton())
            {
                api = "VKD3D";
            }
            else
            {
                switch (state.swapchainApi)
                {
                case Vulkan:
                    api = "VLK";
                    break;

                case DX11:
                    api = "D3D11";
                    break;

                case DX12:
                    if (state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12)
                        api = "D3D11 w/DX12";
                    else
                        api = "D3D12";

                    break;

                default:
                    switch (state.api)
                    {
                    case Vulkan:
                        api = "VLK";
                        break;

                    case DX11:
                        api = "D3D11";
                        break;

                    case DX12:
                        api = "D3D12";
                        break;

                    default:
                        api = "???";
                        break;
                    }

                    break;
                }
            }

            // Overlay font and size, the menu's unless set
            const float overlayFontPx = std::round(fpsScale * config->FpsOverlayFontSize.value_or(fontSize));

            if (gOverlayFont != nullptr)
                ImGui::PushFont(gOverlayFont, overlayFontPx);
            else
                ImGui::PushFontSize(overlayFontPx);

            auto fg = state.CurrentFG();
            auto fgText =
                (fg != nullptr && fg->IsActive() && !fg->IsPaused()) ? std::string(fg->Name()) : std::string();

            const int fakeFramesCount = state.dlssgDetectedInterpolationCount;
            auto formatFg = [&](std::string_view name, int maxFakeFrames)
            {
                if (fakeFramesCount > maxFakeFrames)
                    return std::format("{} 不支持超过 {}x", name, maxFakeFrames);

                else if (fakeFramesCount == 0)
                    return std::format("{} 关闭", name);

                return std::format("{} x{}", name, fakeFramesCount + 1);
            };

            const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;

            // Its frames come from the replacement, but the count is its own, not the game's DLSSG one
            if (state.activeFgOutput == FGOutput::FlipMeter)
            {
                if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                    fgText = std::format("{} x{}", fg->Name(), fg->GetInterpolatedFrameCount() + 1);
                else
                    fgText = "翻转节流关闭";
            }
            else if (activeNvngxFg == FGNvngxReplacement::Arturs)
            {
                fgText = formatFg("Enabler", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                fgText = formatFg("Nukems", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::FFX)
            {
                fgText = formatFg("FFX", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Combo)
            {
                fgText = formatFg("组合", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Custom)
            {
                fgText = formatFg(state.activeFgNvngxCustom, Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (state.activeFgOutput == FGOutput::DLSSG && fg)
            {
                fgText = formatFg("DLSSG", fg->GetMaxInterpolationCount());
            }

            const auto overlayType = config->FpsOverlayType.value_or_default();
            const bool hasFeature = currentFeature && !currentFeature->IsFrozen();

            // Lines are made of labels (names, separators, info) and values, each with its own color and size
            struct OverlayPart
            {
                std::string text;
                bool value = false;
                float width = 0.0f;
                std::vector<int> padding; // Digits in front of each number of a value
            };

            struct OverlayLine
            {
                std::vector<OverlayPart> parts;
                ImVec2 size { 0.0f, 0.0f };
                int row = OverlayRow_Fps;
                // A graph row has no parts, nor the low latency row
                const char* graphId = nullptr;
                RingBuffer<float, plotWidth>* graphData = nullptr;
                float graphMax = 0.0f;
                bool lowLatency = false;

                OverlayLine& Label(std::string text)
                {
                    if (!text.empty())
                        parts.push_back({ std::move(text), false });

                    return *this;
                }

                // The id keeps the value's widest width across frames
                OverlayLine& Value(const char* id, std::string text)
                {
                    auto padding = OverlayValuePadding(id, text);
                    parts.push_back({ std::move(text), true, 0.0f, std::move(padding) });
                    return *this;
                }
            };

            const bool compact = config->FpsOverlayCompactLabels.value_or_default();
            auto L = [compact](const char* full, const char* shorter) { return std::string(compact ? shorter : full); };
            const bool averages = !config->FpsOverlayHideAverages.value_or_default();

            std::string featureText;

            if (hasFeature)
            {
                const bool usesDx12CompatLayer = currentFeature->IsWithDx12();

                featureText = StrFmt("%s -> %s%s", ApiUpscalerInputName(state.currentInputApiName).c_str(),
                                     NameWithVersion(currentFeature).c_str(), usesDx12CompatLayer ? " 搭配 Dx12" : "");
            }

            uint32_t interpolatedFrameCount = 0;
            if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                interpolatedFrameCount = fg->GetInterpolatedFrameCount();

            // The values that change every frame, as their average over the update interval so they can be read
            struct ShownTimes
            {
                double frameRate = 0.0;
                double frameTime = 0.0;
                double averageFrameTime = 0.0;
                double lastFrameTime = 0.0;
                double lastUpscalerTime = 0.0;
                double averageUpscalerTime = 0.0;
            };

            static ShownTimes shown;
            static ShownTimes summed;
            static uint32_t summedFrames = 0;
            static double shownAt = 0.0;

            summed.frameRate += frameRate;
            summed.frameTime += frameTime;
            summed.averageFrameTime += averageFrameTime;
            summed.lastFrameTime += state.frameTimes.empty() ? 0.0 : state.frameTimes.back();
            summed.lastUpscalerTime += state.upscaleTimes.empty() ? 0.0 : state.upscaleTimes.back();
            summed.averageUpscalerTime += averageUpscalerFT;
            summedFrames++;

            const float updateIntervalMs = config->FpsOverlayUpdateInterval.value_or_default();

            if (updateIntervalMs < 1.0f || shownAt <= 0.001 || now < shownAt || now - shownAt >= updateIntervalMs)
            {
                const double frames = static_cast<double>(summedFrames);
                shown = { summed.frameRate / frames,        summed.frameTime / frames,
                          summed.averageFrameTime / frames, summed.lastFrameTime / frames,
                          summed.lastUpscalerTime / frames, summed.averageUpscalerTime / frames };

                summed = {};
                summedFrames = 0;
                shownAt = now;
            }

            // With frame generation also the base frame rate
            const std::string fpsValue =
                interpolatedFrameCount
                    ? StrFmt("%.1f/%.1f", shown.frameRate, shown.frameRate / (double) (interpolatedFrameCount + 1))
                    : StrFmt("%.1f", shown.frameRate);

            const auto overlayRows = OverlayRows(overlayType);
            std::array<bool, OverlayRow_COUNT> rowShown {};

            for (const auto& entry : overlayRows)
                rowShown[entry.row] = entry.shown;

            // Low latency timings, polled while their row is shown
            constexpr auto delayBetweenPollsMs = 500;
            static auto previousPoll = 0.0;
            static bool gotData = false;
            static TimingData timingData {};

            if (rowShown[OverlayRow_LowLatency] && (previousPoll <= 0.001 || previousPoll + delayBetweenPollsMs < now))
            {
                gotData = InputCommon::get_timing_data(timingData);
                previousPoll = now;
            }

            std::array<std::optional<OverlayLine>, OverlayRow_COUNT> rows;

            const auto fpsRow = OverlayFpsRow(overlayType);

            if (fpsRow == FpsOverlayFpsRow_JustFps)
            {
                rows[OverlayRow_Fps] = OverlayLine().Value("fps", fpsValue).Label(" ");
            }
            else
            {
                OverlayLine line;
                line.Label(api + L(" | FPS: ", " | FPS ")).Value("fps", fpsValue);

                if (fpsRow == FpsOverlayFpsRow_FrameTime)
                    line.Label(", ").Value("fpsFrameTime", StrFmt("%.2f ms", shown.frameTime));
                else if (averages)
                    line.Label(L(", Avg: ", " Avg "))
                        .Value("fpsAverage", StrFmt("%.1f", 1000.0 / shown.averageFrameTime));

                if (!fgText.empty())
                    line.Label(" (" + fgText + ")");

                if (!featureText.empty())
                    line.Label(" | " + featureText);

                rows[OverlayRow_Fps] = std::move(line);
            }

            if (hasFeature && currentFeature->RenderWidth() > 0)
            {
                const float ratio = (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();

                OverlayLine line;
                line.Label(L("Upscale: ", "UPS "))
                    .Value("upscale", StrFmt("%ux%u -> %ux%u (%.2fx)", currentFeature->RenderWidth(),
                                             currentFeature->RenderHeight(), currentFeature->TargetWidth(),
                                             currentFeature->TargetHeight(), ratio));

                rows[OverlayRow_Upscale] = std::move(line);
            }

            auto graphRow = [](const char* id, RingBuffer<float, plotWidth>* data, float max)
            {
                OverlayLine line;
                line.graphId = id;
                line.graphData = data;
                line.graphMax = max;

                return line;
            };

            rows[OverlayRow_FrameTime] = OverlayLine()
                                             .Label(L("Frame Time: ", "FT "))
                                             .Value("frameTime", StrFmt("%.2f ms", shown.lastFrameTime));

            if (averages)
                rows[OverlayRow_FrameTime]
                    ->Label(L(", Avg: ", " Avg "))
                    .Value("frameTimeAverage", StrFmt("%.2f ms", shown.averageFrameTime));

            rows[OverlayRow_FrameTimeGraph] = graphRow("##FrameTimeGraph", &gFrameTimes, 66.6f);

            rows[OverlayRow_UpscalerTime] = OverlayLine()
                                                .Label(L("Upscaler Time: ", "UT "))
                                                .Value("upscalerTime", StrFmt("%.2f ms", shown.lastUpscalerTime));

            if (averages)
                rows[OverlayRow_UpscalerTime]
                    ->Label(L(", Avg: ", " Avg "))
                    .Value("upscalerTimeAverage", StrFmt("%.2f ms", shown.averageUpscalerTime));

            rows[OverlayRow_UpscalerTimeGraph] = graphRow("##UpscalerFrameTimeGraph", &gUpscalerTimes, 20.0f);

            if (gotData && timingData.timeRange.has_value())
            {
                rows[OverlayRow_LowLatency] = OverlayLine();
                rows[OverlayRow_LowLatency]->lowLatency = true;
            }

            std::vector<OverlayLine> lines;
            for (const auto& entry : overlayRows)
            {
                if (entry.shown && rows[entry.row].has_value())
                {
                    lines.push_back(std::move(rows[entry.row].value()));
                    lines.back().row = entry.row;
                }
            }

            ImFont* font = ImGui::GetFont();
            const float basePx = ImGui::GetFontSize();
            const float labelPx = std::round(basePx * config->FpsOverlayLabelScale.value_or_default());
            const float valuePx = std::round(basePx * config->FpsOverlayValueScale.value_or_default());

            const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_Text);
            const ImU32 valueColor = config->OverlaysUseTheme.value_or_default()
                                         ? labelColor
                                         : ImGui::ColorConvertFloat4ToU32(toneMapColor(
                                               RgbToColor(config->FpsOverlayValueColor.value_or_default())));

            // A stronger shadow is thicker and darker
            const auto shadowType = config->FpsOverlayShadowType.value_or_default();
            const float shadowStrength = config->FpsOverlayShadowStrength.value_or_default();
            const float shadowSize = std::max(1.0f, std::round(basePx / 14.0f * (0.5f + shadowStrength * 1.5f)));
            auto shadowVec = toneMapColor(RgbToColor(config->FpsOverlayShadowColor.value_or_default()));
            shadowVec.w = 0.35f + 0.65f * shadowStrength;
            const ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32(shadowVec);

            // The outline is a filled circle of opaque copies around the text, translucent ones would add up unevenly
            std::vector<ImVec2> outlineOffsets;

            if (shadowType == FpsOverlayShadow_Outline)
            {
                const int radius = static_cast<int>(shadowSize);
                const int limit = radius * radius + radius;

                for (int dy = -radius; dy <= radius; dy++)
                {
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        if ((dx != 0 || dy != 0) && dx * dx + dy * dy <= limit)
                            outlineOffsets.emplace_back(static_cast<float>(dx), static_cast<float>(dy));
                    }
                }

                shadowVec.w = 1.0f;
            }

            const ImU32 outlineColor = ImGui::ColorConvertFloat4ToU32(shadowVec);

            auto partPx = [&](const OverlayPart& part) { return part.value ? valuePx : labelPx; };

            // Digits of the values take the widest digit's width, so changing numbers don't shift with fonts whose
            // digits differ in width
            float digitWidth = 0.0f;
            for (char digit = '0'; digit <= '9'; digit++)
                digitWidth = std::max(digitWidth, font->CalcTextSizeA(valuePx, FLT_MAX, 0.0f, &digit, &digit + 1).x);

            // Calls place(begin, end, x) for the part's text, the value's digits one at a time centered in their
            // width and each number after its padding. The part's width.
            auto layoutPart = [&](const OverlayPart& part, auto&& place)
            {
                const float px = partPx(part);
                const char* begin = part.text.c_str();
                const char* text = begin;
                const char* end = text + part.text.size();
                float x = 0.0f;
                size_t number = 0;

                auto isDigit = [&](const char* c) { return part.value && *c >= '0' && *c <= '9'; };

                while (text < end)
                {
                    if (isDigit(text))
                    {
                        if ((text == begin || !isDigit(text - 1)) && number < part.padding.size())
                            x += part.padding[number++] * digitWidth;

                        const float width = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text, text + 1).x;
                        place(text, text + 1, x + std::round((digitWidth - width) * 0.5f));
                        x += digitWidth;
                        text++;
                        continue;
                    }

                    const char* runEnd = text;
                    while (runEnd < end && !isDigit(runEnd))
                        runEnd++;

                    place(text, runEnd, x);
                    x += font->CalcTextSizeA(px, FLT_MAX, 0.0f, text, runEnd).x;
                    text = runEnd;
                }

                return x;
            };

            auto measureLine = [&](OverlayLine& line)
            {
                ImVec2 size { 0.0f, 0.0f };

                for (auto& part : line.parts)
                {
                    const float px = partPx(part);
                    part.width = layoutPart(part, [](const char*, const char*, float) {});
                    size.x += part.width;
                    size.y = std::max(size.y, px);
                }

                line.size = size;
            };

            for (auto& line : lines)
                measureLine(line);

            // On whole pixels, the shadows of a line under all its text so they don't cover the next part
            auto drawText = [&](ImDrawList* drawList, float px, ImVec2 pos, ImU32 color, const char* text,
                                const char* end, bool shadowPass)
            {
                pos = ImVec2(std::round(pos.x), std::round(pos.y));

                if (!shadowPass)
                {
                    drawList->AddText(font, px, pos, color, text, end);
                }
                else if (shadowType == FpsOverlayShadow_Shadow)
                {
                    for (float d = 1.0f; d <= shadowSize; d++)
                        drawList->AddText(font, px, ImVec2(pos.x + d, pos.y + d), shadowColor, text, end);
                }
                else if (shadowType == FpsOverlayShadow_Outline)
                {
                    for (const auto& offset : outlineOffsets)
                        drawList->AddText(font, px, pos + offset, outlineColor, text, end);
                }
            };

            auto lineAscent = [&](const OverlayLine& line)
            {
                float ascent = 0.0f;
                for (const auto& part : line.parts)
                    ascent = std::max(ascent, font->GetFontBaked(partPx(part))->Ascent);

                return ascent;
            };

            // For a horizontal overlay, where the lines side by side share one baseline
            float sharedAscent = 0.0f;

            // Parts of different sizes share the baseline
            auto drawLine = [&](OverlayLine& line)
            {
                if (line.size.y == 0.0f && !line.parts.empty())
                    measureLine(line);

                const float maxAscent = std::max(lineAscent(line), sharedAscent);

                auto drawList = ImGui::GetWindowDrawList();
                const auto start = ImGui::GetCursorScreenPos();

                for (const bool shadowPass : { true, false })
                {
                    if (shadowPass && shadowType == FpsOverlayShadow_Off)
                        continue;

                    auto pos = start;

                    for (const auto& part : line.parts)
                    {
                        const float px = partPx(part);
                        const float y = pos.y + maxAscent - font->GetFontBaked(px)->Ascent;
                        const ImU32 color = part.value ? valueColor : labelColor;

                        layoutPart(part, [&](const char* text, const char* end, float x)
                                   { drawText(drawList, px, ImVec2(pos.x + x, y), color, text, end, shadowPass); });

                        pos.x += part.width;
                    }
                }

                ImGui::Dummy(ImVec2(line.size.x, line.size.y + maxAscent - lineAscent(line)));
            };

            const bool horizontal = config->FpsOverlayHorizontal.value_or_default();
            const float graphHeight = config->FpsOverlayGraphHeight.value_or_default();

            ImVec2 plotSize;
            if (horizontal)
            {
                plotSize = { fpsScale * 150, fpsScale * 16 * graphHeight };
            }
            else
            {
                // Graphs match the widest text line
                auto textWidth = 0.0f;
                for (const auto& line : lines)
                    textWidth = std::max(textWidth, line.size.x);

                plotSize = { std::max(textWidth, fpsScale * 300.0f), fpsScale * 30 * graphHeight };
            }

            // Rows are apart by the row spacing, by default as ImGui::Spacing() puts them
            const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
            const float rowSpacing = config->FpsOverlayRowSpacing.has_value()
                                         ? config->FpsOverlayRowSpacing.value() * fpsScale
                                         : itemSpacingY * 2.0f;

            // The timings as lines of text, as wide as the graphs so their bars fit
            const ImVec2 itemSpacing = ImGui::GetStyle().ItemSpacing;

            auto drawLowLatency = [&]()
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
                ImGui::BeginGroup();

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                constexpr float offsetForText = 155;

                const auto& rangeInNs = timingData.timeRange.value().length;

                UINT64 localFrameCount = 0;

                if (fg != nullptr)
                    localFrameCount = fg->FrameCount();

                // Text lines with the overlay's shadow or outline under them
                auto shadowedText = [&](ImU32 color, const std::string& text)
                {
                    const char* begin = text.c_str();
                    const char* end = begin + text.size();
                    drawText(drawList, ImGui::GetFontSize(), ImGui::GetCursorScreenPos(), color, begin, end, true);

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(begin, end);
                    ImGui::PopStyleColor();
                };

                // OverlayLine frameIds;
                // frameIds.Label("FGId: ")
                //     .Value("lowLatencyFgId", StrFmt("%llu", localFrameCount))
                //     .Label(", RfxId: ")
                //     .Value("lowLatencyRfxId", StrFmt("%llu", state.reflexFrameId));
                // drawLine(frameIds);
                OverlayLine wholeFrame;
                wholeFrame.Label("低延迟计时，整帧：")
                    .Value("lowLatencyFrame", StrFmt("%.1fms", rangeInNs / 1000.0));
                drawLine(wholeFrame);

                const auto maxWidth = horizontal ? fpsScale * 300.0f : plotSize.x;

                const auto drawTiming = [&](const auto& timingOpt, const char* desc, ImVec4 color)
                {
                    if (!timingOpt.has_value())
                        return;

                    auto toneMappedColor = toneMapColor(color);

                    const auto& timing = timingOpt.value();
                    float duration = static_cast<float>(timing.length * rangeInNs / 1000.0);

                    shadowedText(ImGui::ColorConvertFloat4ToU32(toneMappedColor),
                                 StrFmt("%-12s %4.1fms", desc, duration));

                    auto leftLimit = ImGui::GetItemRectMin().x + offsetForText * fpsScale;

                    auto start = static_cast<float>(leftLimit + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                    timing.position);

                    auto end =
                        static_cast<float>(start + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) * timing.length);

                    auto pos = ImVec2(start, ImGui::GetItemRectMin().y);
                    auto size = ImVec2(end, ImGui::GetItemRectMax().y);

                    drawList->AddRectFilled(pos, size, ImGui::ColorConvertFloat4ToU32(toneMappedColor));
                };

                drawTiming(timingData.simulation, "模拟", ImVec4(0.768f, 0.169f, 0.169f, 1.0f));
                drawTiming(timingData.renderSubmit, "渲染提交", ImVec4(0.235f, 0.705f, 0.294f, 1.0f));
                drawTiming(timingData.present, "呈现", ImVec4(1.0f, 0.88f, 0.098f, 1.0f));
                drawTiming(timingData.driver, "驱动", ImVec4(0.263f, 0.388f, 0.847f, 1.0f));
                drawTiming(timingData.osRenderQueue, "渲染队列", ImVec4(0.76f, 0.51f, 0.188f, 1.0f));
                drawTiming(timingData.gpuRender, "GPU 渲染", ImVec4(0.569f, 0.117f, 0.705f, 1.0f));

                // The bars are drawn without taking space
                ImGui::Dummy(ImVec2(maxWidth, 0.0f));

                ImGui::EndGroup();
                ImGui::PopStyleVar();
            };

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(itemSpacing.x, 0.0f));

            OverlayLine sepLine;
            if (horizontal)
            {
                sepLine.Label(" | ");
                measureLine(sepLine);

                for (const auto& line : lines)
                    sharedAscent = std::max(sharedAscent, lineAscent(line));
            }

            // Draw the overlay
            for (size_t i = 0; i < lines.size(); i++)
            {
                auto& line = lines[i];

                // A graph right after its text row stays with it
                const bool attached = line.graphData != nullptr && i > 0 && lines[i - 1].row == line.row - 1;

                if (i > 0)
                {
                    if (horizontal)
                    {
                        ImGui::SameLine(0.0f, 0.0f);

                        if (!attached)
                        {
                            drawLine(sepLine);
                            ImGui::SameLine(0.0f, 0.0f);
                        }
                    }
                    else
                    {
                        ImGui::Dummy(ImVec2(0.0f, attached ? itemSpacingY : rowSpacing));
                    }
                }

                if (line.graphData != nullptr)
                {
                    ImGui::PlotLines(
                        line.graphId, [](void* rb, int idx) -> float
                        { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, line.graphData, plotWidth,
                        0, nullptr, 0.0f, line.graphMax, plotSize);
                }
                else if (line.lowLatency)
                {
                    drawLowLatency();
                }
                else
                {
                    drawLine(line);
                }
            }

            ImGui::PopStyleVar();

            if (gOverlayFont != nullptr)
                ImGui::PopFont();
            else
                ImGui::PopFontSize();
        }

        // Restore the style
        if (!config->OverlaysUseTheme.value_or_default())
            ImGui::PopStyleColor(5);
        else
            ImGui::PopStyleColor(2);

        // Get size for postioning
        overlaySize = ImGui::GetWindowSize();

        ImGui::End();

        ImGui::PopStyleVar(); // Rounding

        if (stylePushed)
            ImGui::PopStyleVar(7);

        // Where the overlay sits in the free space for each FpsOverlayPos, 0 left/top to 1 right/bottom
        static constexpr ImVec2 alignments[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f },
                                                 { 1.0f, 1.0f }, { 0.5f, 0.0f }, { 0.5f, 1.0f } };

        const auto position = config->FpsOverlayPosition.value_or_default();
        const float alignX =
            position == FpsOverlayPos_Custom ? config->FpsOverlayPosX.value_or_default() : alignments[position].x;
        const float alignY =
            position == FpsOverlayPos_Custom ? config->FpsOverlayPosY.value_or_default() : alignments[position].y;

        // Prevent overlapping with splash message at the bottom
        auto freeHeight = io.DisplaySize.y - overlaySize.y;
        if (alignY > 0.5f && !config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit)
            freeHeight -= splashSize.y;

        overlayPosition.x = std::round(std::max(0.0f, io.DisplaySize.x - overlaySize.x) * alignX);
        overlayPosition.y = std::round(std::max(0.0f, freeHeight) * alignY);
    }
}

void MenuCommon::RenderMainMenuHeaderMessages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;
    auto& versionStatus = ctx.versionStatus;
    auto& currentVersionText = ctx.currentVersionText;

    if (!_showMipmapCalcWindow && !_showHudlessWindow && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
        ImGui::SetWindowFocus();

    if (config->MenuScale.has_value())
    {
        _selectedScale = ((int) (menuResScale * 10.0f)) - 4;
    }
    else
    {
        _selectedScale = 0;
    }

    if (versionStatus.completed)
    {
        if (versionStatus.updateAvailable && !versionStatus.latestTag.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(StatusWarn(), "有可用更新：%s（当前 %s）", versionStatus.latestTag.c_str(),
                               currentVersionText.c_str());

            if (!versionStatus.latestUrl.empty())
            {
                ImGui::SameLine();
                ImGui::TextLinkOpenURL("打开发布页面", versionStatus.latestUrl.c_str());
            }

            ImGui::Spacing();
        }
        else if (!versionStatus.error.empty())
        {
            LOG_ERROR("版本检查失败：{0}", versionStatus.error);
            versionStatus.error.clear();

            // The status is copied from State every frame
            std::scoped_lock lock(state.versionCheckMutex);
            state.versionCheckError.clear();
        }
        // Disabled error message
        // else if (!versionStatus.error.empty())
        //{
        //    ImGui::Spacing();
        //    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.4f, 0.f, 1.f)), "%s", versionStatus.error.c_str());
        //    ImGui::Spacing();
        //}
    }

    if (state.nvngxIniDetected)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, StatusError());
        ImGui::TextWrapped("检测到 nvngx.ini，请改用 OptiScaler.ini 并删除旧配置");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

void MenuCommon::RenderUpscalerStateMessage(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    // No active upscaler message
    if (currentFeature == nullptr || !currentFeature->IsInited())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.0f));
        else
            ImGui::SetWindowFontScale(menuResScale * 2.0f);

        if (state.nvngxExists || state.nvngxReplacement.has_value() ||
            (state.libxessExists || XeSSProxy::Module() != nullptr))
        {
            ImGui::Spacing();

            std::vector<std::string> upscalers;

            if (state.fsrHooks)
                upscalers.push_back("FSR");

            if (state.nvngxExists || state.nvngxReplacement.has_value() || primaryGpu.dlssCapable)
                upscalers.push_back("DLSS");

            if (state.libxessExists || XeSSProxy::Module() != nullptr)
                upscalers.push_back("XeSS");

            auto joined = upscalers | std::views::join_with(std::string { " 或 " });

            std::string joinedUpscalers(joined.begin(), joined.end());

            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text("请在游戏选项中选择 %s 作为超分器并加载存档，以启用 Opti 设置。\n"
                        "超分器在菜单中并不总是有效。",
                        joinedUpscalers.c_str());
            ImGui::PopTextWrapPos();

            if (config->UseHQFont.value_or_default())
                ImGui::PopFontSize();
            else
                ImGui::SetWindowFontScale(menuResScale);

            ImGui::Spacing();

            RenderDetectedCard(ctx);

            ImGui::Spacing();
        }
        else
        {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text("找不到 nvngx.dll、libxess.dll 和 FSR 输入\n超分支持将无法工作。");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();

            if (config->UseHQFont.value_or_default())
                ImGui::PopFont();
            else
                ImGui::SetWindowFontScale(menuResScale);

            RenderDetectedCard(ctx);
        }
    }
    else if (currentFeature->IsFrozen())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.0f));
        else
            ImGui::SetWindowFontScale(menuResScale * 2.0f);

        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("%s 已激活，但游戏当前未使用\n请进入游戏",
                    currentFeature->Name().c_str());
        ImGui::PopTextWrapPos();

        if (config->UseHQFont.value_or_default())
            ImGui::PopFont();
        else
            ImGui::SetWindowFontScale(menuResScale);
    }
}

static const char* ApiName(API api);

void MenuCommon::RenderActiveUpscalerSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    // UPSCALERS -----------------------------
    SectionTitle("超分器");

    GetCurrentBackendInfo(state.api, currentBackend, &currentBackendName);

    ImGui::PushItemWidth(150.0f * menuResScale);

    const bool usesDlssd = currentFeature->GetUpscalerType() == Upscaler::DLSSD;
    const bool rayReconstruction = state.rrHandles.contains(currentFeature->Handle()->Id);

    // Ray reconstruction picks its DLSSD, DX11's runs only on NVIDIA's
    if (rayReconstruction && (state.api == DX12 || state.api == Vulkan))
    {
        RenderRRUpscalerCombo(state.api, currentFeature);
    }
    else if (usesDlssd)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(NameWithVersion(currentFeature).c_str());
    }
    else
    {
        switch (state.api)
        {
        case DX11:
            AddDx11Backends(currentBackend);
            break;

        case DX12:
            AddDx12Backends(currentBackend);
            break;

        default:
            AddVulkanBackends(currentBackend);
        }
    }

    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::TextDisabled("%dx%d -> %dx%d (%.2fx)", currentFeature->RenderWidth(), currentFeature->RenderHeight(),
                        currentFeature->TargetWidth(), currentFeature->TargetHeight(),
                        (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth());

    // The game leaves ray traced effects noisy for ray reconstruction to clean up
    if (rayReconstruction && !usesDlssd && config->RRAllowRegularUpscalers.value_or_default())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(
            "游戏已开启 DLSSD，但此超分器无法降噪。\n画面可能出现颗粒或噪点。");
        ImGui::PopStyleColor();
    }

    // Ray reconstruction's upscaler can be picked before the game starts it, shown for games that look for it
    if (!rayReconstruction && state.rrRequested && config->RRAllowRegularUpscalers.value_or_default() &&
        (state.api == DX12 || state.api == Vulkan))
    {
        ImGui::PushItemWidth(150.0f * menuResScale);
        RenderRROfferCombo(state.api);
        ImGui::PopItemWidth();
    }

    if (currentFeature->DynamicResolution())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("（DRS）");
    }

    if (!usesDlssd && !rayReconstruction)
    {
        if (state.newBackend != Upscaler::Reset && state.newBackend != currentBackend)
        {
            if (state.newBackend == Upscaler::XeSS)
            {
                // Reseting them for xess
                config->DisableReactiveMask.reset();
                config->DlssReactiveMaskBias.reset();
            }

            MARK_ALL_BACKENDS_CHANGED();
        }
    }

    if (primaryGpu.dlssCapable && !state.NVNGX_DLSS_Path.has_value())
    {
        ImGui::Spacing();
        ImGui::TextColored(StatusWarn(), "未找到 nvngx_dlss.dll，DLSS 已禁用！");
    }

    // Dx11 with Dx12
    if (state.api == DX11 && currentFeature->IsWithDx12())
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("Dx11 搭配 Dx12 设置"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            if (bool dontUseNTShared = config->DontUseNTShared.value_or_default();
                ImGui::Checkbox("不使用 NTShared", &dontUseNTShared))
                config->DontUseNTShared = dontUseNTShared;

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    if (state.api == Vulkan && currentFeature->IsWithDx12())
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("Vulkan 搭配 Dx12 设置"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            if (bool inputsUseCopy = config->VulkanUseCopyForInputs.value_or_default();
                ImGui::Checkbox("输入使用 CopyResource", &inputsUseCopy))
                config->VulkanUseCopyForInputs = inputsUseCopy;

            if (bool outputUseCopy = config->VulkanUseCopyForOutput.value_or_default();
                ImGui::Checkbox("输出使用 CopyResource", &outputUseCopy))
                config->VulkanUseCopyForOutput = outputUseCopy;

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    // UPSCALER SPECIFIC -----------------------------

    // XeSS -----------------------------
    if (currentBackend == Upscaler::XeSS && !usesDlssd)
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("XeSS 设置"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            const char* models[] = { "KPSS", "SPLAT", "MODEL_3", "MODEL_4", "MODEL_5", "MODEL_6" };
            auto configModes = config->NetworkModel.value_or_default();

            if (configModes < 0 || configModes > 5)
                configModes = 0;

            const char* selectedModel = models[configModes];

            if (ImGui::BeginCombo("网络模型", selectedModel))
            {
                for (int n = 0; n < 6; n++)
                {
                    if (ImGui::Selectable(models[n], (config->NetworkModel.value_or_default() == n)))
                    {
                        config->NetworkModel = n;
                        state.newBackend = SameBackend(currentBackend);
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                }

                ImGui::EndCombo();
            }
            ShowTooltip("可能没什么作用");

            if (bool dbg = state.xessDebug; ImGui::Checkbox("转储（Shift+Del）", &dbg))
                state.xessDebug = dbg;

            ImGui::SameLine(0.0f, 6.0f);
            int dbgCount = state.xessDebugFrames;

            ImGui::PushItemWidth(95.0f * menuResScale);
            if (ImGui::InputInt("帧", &dbgCount))
            {
                if (dbgCount < 4)
                    dbgCount = 4;
                else if (dbgCount > 999)
                    dbgCount = 999;

                state.xessDebugFrames = dbgCount;
            }

            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    constexpr auto fsr3DebugViewTooltip = "左上：膨胀运动矢量\n"
                                          "中上：受保护区域\n"
                                          "右上：膨胀深度\n"
                                          "中间：超分后的画面\n"
                                          "左下：去遮挡蒙版\n"
                                          "中下：反应度\n"
                                          "右下：细节保护衰减";

    // FFX -----------------
    if (!usesDlssd && (currentBackend == Upscaler::FFX || currentBackend == Upscaler::FFX_on12))
    {
        const auto customName = CustomProviders::Selected(state.api, currentBackend);
        const auto customUpscaler = SelectedCustomFfx(customName, currentBackend, state.api);
        auto& upscalerIndex = config->FfxUpscalerIndexFor(customName);

        // A custom upscaler only has a list once it got loaded
        if (auto versions = customName.empty()          ? GetFfxVersionList(currentBackend, state.api)
                            : customUpscaler != nullptr ? &customUpscaler->versions
                                                        : nullptr;
            versions != nullptr && !versions->names.empty())
        {
            ImGui::Spacing();

            const int ffxIndex = upscalerIndex.value_or_default();
            auto currentName =
                ffxIndex >= 0 && ffxIndex < versions->names.size() ? StrFmt("FSR %s", versions->names[ffxIndex]) : "";

            ImGui::PushItemWidth(150.0f * menuResScale);

            if (ImGui::BeginCombo("FFX 超分器", currentName.c_str()))
            {
                for (auto n : FfxVersionsNewestFirst(*versions))
                {
                    if (ImGui::Selectable(StrFmt("FSR %s##%d", versions->names[n], n).c_str(), ffxIndex == n) &&
                        ffxIndex != n)
                    {
                        upscalerIndex = n;
                        state.newBackend = SameBackend(currentBackend);
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            ShowTooltip("FFX SDK 报告的超分器列表");
        }

        auto majorFsrVersion = currentFeature->Version().major;

        if (majorFsrVersion >= 4)
        {
            ImGui::Spacing();

            // Colorspaces
            const char* colorSpaces[] = { "线性（默认）", "非线性", "非线性 sRGB", "非线性 PQ" };
            int currentColorSpace = 0;
            if (config->FsrNonLinearPQ.value_or_default())
                currentColorSpace = 3;
            else if (config->FsrNonLinearSRGB.value_or_default())
                currentColorSpace = 2;
            else if (config->FsrNonLinearColorSpace.value_or_default())
                currentColorSpace = 1;

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo("输入色彩空间", &currentColorSpace, colorSpaces, IM_ARRAYSIZE(colorSpaces)))
            {
                bool isSrgb = (currentColorSpace == 2);
                bool isPq = (currentColorSpace == 3);

                config->FsrNonLinearSRGB = isSrgb;
                config->FsrNonLinearPQ = isPq;

                if (isSrgb || isPq)
                {
                    config->FsrNonLinearColorSpace.set_volatile_value(true);
                }
                else if (currentColorSpace == 1) // Just non-Linear
                {
                    config->FsrNonLinearColorSpace = true;
                }
                else // Linear
                {
                    config->FsrNonLinearColorSpace = false;
                }

                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
            ShowTooltip("选择游戏使用的输入色彩空间。\n"
                        "非线性 / sRGB：可能改善 FSR4 超分质量，但也可能增加拖影。\n"
                        "PQ：最罕见，可能增加拖影并破坏灯光。");

            // FSR 4 Presets
            const char* presets[] = {
                "默认", "预设 0", "预设 1", "预设 2", "预设 3", "预设 4", "预设 5"
            };
            int currentPresetIdx = config->Fsr4Preset.has_value() ? config->Fsr4Preset.value() + 1 : 0;

            if (currentPresetIdx < 0 || currentPresetIdx >= IM_ARRAYSIZE(presets))
                currentPresetIdx = 0;

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo("FSR4 预设", &currentPresetIdx, presets, IM_ARRAYSIZE(presets)))
            {
                if (currentPresetIdx == 0)
                    config->Fsr4Preset.reset();
                else
                    config->Fsr4Preset = currentPresetIdx - 1;

                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
            ShowTooltip("每个内部 FSR4 预设都针对特定分辨率调优。\n"
                        "选择 FSR4 预设不会改变游戏内的\n超分器预设！！！\n\n"
                        "预设 0 适用于 FSR 原生抗锯齿\n"
                        "预设 1 适用于质量/极致质量\n"
                        "预设 2 适用于均衡\n"
                        "预设 3 适用于性能\n"
                        "预设 4 适用于 DRS\n"
                        "预设 5 适用于极致性能");

            // Display the active preset right next to the combo box instead of using a table
            // The active preset can be left over from another module, so a custom one checks its own hook
            ImGui::SameLine();
            if (customUpscaler != nullptr && !FSR4Presets::IsModuleHooked(customUpscaler->module.dll))
                ImGui::TextDisabled("（挂钩失败）");
            else if (state.currentFsr4Preset.has_value())
                ImGui::TextDisabled("（当前启用：%d）", state.currentFsr4Preset.value());
            else if (FSR4Presets::IsAnyModuleHooked())
                ImGui::TextColored(StatusWarn(), "（可能是 FSR3 回退）");
            else
                ImGui::TextDisabled("（挂钩失败）");

            // FSR 4 Models of the module running the selected upscaler, only listed when the GPU can run more than one
            HMODULE upscalerModule = nullptr;
            if (customUpscaler != nullptr)
                upscalerModule = customUpscaler->module.dll;
            else if (auto versions = GetFfxVersionList(currentBackend, state.api);
                     versions != nullptr && config->FfxUpscalerIndex.value_or_default() >= 0 &&
                     config->FfxUpscalerIndex.value_or_default() < versions->ids.size())
            {
                auto versionId = versions->ids[config->FfxUpscalerIndex.value_or_default()];

                if (versions == &state.ffxUpscalerVersionsDx12)
                    upscalerModule = FfxApiProxy::UpscalerModule(versionId);
                else if (versions == &state.ffxUpscalerVersionsVk)
                    upscalerModule = FfxApiProxy::UpscalerModuleVk(versionId);
            }

            if (auto models = FSR4Models::SelectableModels(upscalerModule); !models.empty())
            {
                auto& selectedModel = config->Fsr4Model;
                auto selectedName = selectedModel.has_value() ? FSR4Models::Name(selectedModel.value()) : "自动";
                bool modelChanged = false;

                ImGui::SetNextItemWidth(150.0f * menuResScale);
                if (ImGui::BeginCombo("FSR4 模型", selectedName))
                {
                    if (ImGui::Selectable("自动", !selectedModel.has_value()) && selectedModel.has_value())
                    {
                        selectedModel.reset();
                        modelChanged = true;
                    }

                    for (auto model : models)
                    {
                        if (ImGui::Selectable(FSR4Models::Name(model), selectedModel == model) &&
                            selectedModel != model)
                        {
                            selectedModel = model;
                            modelChanged = true;
                        }
                    }

                    ImGui::EndCombo();
                }

                if (modelChanged)
                {
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }

                ShowTooltip("要运行的 FSR 4 网络。\n"
                            "自动会让 FFX 选择 GPU 支持的最佳网络。");

                ImGui::SameLine();
                if (auto activeModel = FSR4Models::ActiveModel(upscalerModule))
                    ImGui::TextDisabled("（当前启用：%s）", FSR4Models::Name(*activeModel));
                else
                    ImGui::TextColored(StatusWarn(), "（可能是 FSR3 回退）");
            }
        }

        if (majorFsrVersion >= 3)
        {
            ImGui::Spacing();

            bool debugView = config->FsrDebugView.value_or_default();
            if (ImGui::Checkbox("超分器调试视图", &debugView))
            {
                config->FsrDebugView = debugView;

                // FSR 4's debug view requires backend reinit
                if (majorFsrVersion > 3)
                {
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }
            }

            if (majorFsrVersion > 3)
            {
                ShowTooltip("左上：膨胀运动矢量\n"
                            "右上：预测混合因子");
            }
            else
            {
                ShowTooltip(fsr3DebugViewTooltip);
            }

            if (majorFsrVersion > 3)
            {
                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                bool fsr4wm = config->Fsr4EnableWatermark.value_or_default();
                if (ImGui::Checkbox("水印", &fsr4wm))
                {
                    LOG_DEBUG("FSR4 水印设为 {}", fsr4wm);
                    config->Fsr4EnableWatermark = fsr4wm;
                    Util::SetFlagEnvVar(L"MLSR-WATERMARK", fsr4wm);

                    // The driver reads the variable on context creation
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }
            }
        }

        if (currentFeature->Version() >= feature_version { 3, 1, 1 } &&
            currentFeature->Version() < feature_version { 4, 0, 0 })
        {
            ImGui::Spacing();

            if (currentFeature != nullptr)
            {
                ImGui::Text("FSR 3.1 预设：");

                ImGui::SameLine(0.0f, 6.0f);

                // This will be applied by default
                if (ImGui::Button("稳定"))
                {
                    auto const scaleRatioX =
                        (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();
                    auto const scaleRatioY =
                        (float) currentFeature->TargetHeight() / (float) currentFeature->RenderHeight();
                    auto const scaleRatio = std::max(scaleRatioX, scaleRatioY);

                    config->FsrVelocity = 0.5f;
                    config->FsrReactiveScale = 0.25f;

                    config->FsrShadingScale.reset();
                    config->FsrAccAddPerFrame.reset();
                    config->FsrMinDisOccAcc.reset();
                    config->FsrShadingScale.set_volatile_value(0.5f / scaleRatio);
                    config->FsrAccAddPerFrame.set_volatile_value(scaleRatio / 10.0f);
                    config->FsrMinDisOccAcc.set_volatile_value(scaleRatio / 20.0f);
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("运动"))
                {
                    auto const scaleRatioX =
                        (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();
                    auto const scaleRatioY =
                        (float) currentFeature->TargetHeight() / (float) currentFeature->RenderHeight();
                    auto const scaleRatio = std::max(scaleRatioX, scaleRatioY);

                    config->FsrVelocity = 1.0f;
                    config->FsrReactiveScale = 0.5f;

                    config->FsrShadingScale.reset();
                    config->FsrAccAddPerFrame.reset();
                    config->FsrMinDisOccAcc.reset();
                    config->FsrShadingScale.set_volatile_value(1.0f / scaleRatio);
                    config->FsrAccAddPerFrame.set_volatile_value(scaleRatio / 10.0f);
                    config->FsrMinDisOccAcc.set_volatile_value(scaleRatio / 20.0f);
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("默认"))
                {
                    config->FsrVelocity = 1.0f;
                    config->FsrReactiveScale = 1.0f;
                    config->FsrShadingScale = 1.0f;
                    config->FsrAccAddPerFrame = 0.333f;
                    config->FsrMinDisOccAcc = -0.333f;
                }
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("FSR 3 超分器手动调优"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushItemWidth(220.0f * menuResScale);

                float velocity = config->FsrVelocity.value_or_default();
                if (ImGui::SliderFloat("速度因子", &velocity, 0.00f, 1.0f, "%.2f"))
                    config->FsrVelocity = velocity;

                ShowTooltip("0.0f 可改善亮像素的时间稳定性\n"
                            "较低的值在拖影方面更稳定\n"
                            "较高的值更像素化，但拖影更少");

                if (currentFeature->Version() >= feature_version { 3, 1, 4 })
                {
                    // Reactive Scale
                    float reactiveScale = config->FsrReactiveScale.value_or_default();
                    if (ImGui::SliderFloat("反应蒙版缩放", &reactiveScale, 0.0f, 1.0f, "%.3f"))
                        config->FsrReactiveScale = reactiveScale;

                    ShowTooltip("用于开发测试\n"
                                "向反应蒙版写入更大的值可减少拖影。");

                    // Shading Scale
                    float shadingScale = config->FsrShadingScale.value_or_default();
                    if (ImGui::SliderFloat("着色缩放", &shadingScale, 0.0f, 1.0f, "%.3f"))
                        config->FsrShadingScale = shadingScale;

                    ShowTooltip("增大此值会缩放 FSR3.1 计算出的着色\n"
                                "在读取时改变该值可获得更高的反应性。");

                    // Accumulation Added Per Frame
                    float accAddPerFrame = config->FsrAccAddPerFrame.value_or_default();
                    if (ImGui::SliderFloat("每帧累积增量", &accAddPerFrame, 0.0f, 1.0f, "%.3f"))
                        config->FsrAccAddPerFrame = accAddPerFrame;

                    ShowTooltip("对应每帧添加的累积量\n"
                                "在发生去遮挡的像素坐标处，或当\n"
                                "反应蒙版值 > 0.0f 时。减小此值并\n"
                                "将拖影物体（即无运动矢量）绘制到反应蒙版\n"
                                "将值调至接近 1.0f 可减少时间性拖影。\n"
                                "减小此值可能导致更多细小特征像素闪烁。");

                    // Min Disocclusion Accumulation
                    float minDisOccAcc = config->FsrMinDisOccAcc.value_or_default();
                    if (ImGui::SliderFloat("最小去遮挡累积量", &minDisOccAcc, -1.0f, 1.0f, "%.3f"))
                        config->FsrMinDisOccAcc = minDisOccAcc;

                    ShowTooltip("增大此值可减少白色像素的时间性\n"
                                "闪烁，这些摆动的细物体正在去遮挡\n"
                                "且经常相互遮挡。过高的值可能增加拖影。");
                }

                ImGui::PopItemWidth();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }

    // FSR 3.1 Dx11 -----------------
    if (!usesDlssd && currentBackend == Upscaler::FSR31)
    {
        ImGui::Spacing();

        bool debugView = config->FsrDebugView.value_or_default();
        if (ImGui::Checkbox("超分器调试视图", &debugView))
            config->FsrDebugView = debugView;

        ShowTooltip(fsr3DebugViewTooltip);
    }

    // DLSS -----------------
    if ((config->DLSSEnabled.value_or_default() &&
         (currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12) &&
         currentFeature->Version().major > 2) ||
        usesDlssd)
    {
        ImGui::Spacing();

        auto overridden = usesDlssd ? state.dlssdPresetsOverriddenExternally : state.dlssPresetsOverriddenExternally;

        if (overridden)
        {
            ImGui::TextColored(StatusWarn(), "预设已被外部覆盖");
            ShowTooltip("这通常是因为使用了工具\n"
                        "例如 Nvidia App 或 Nvidia Inspector");
            // ImGui::Text("Selecting setting below will disable that external override\n"
            //             "but you need to Save Settings and restart the game");

            ImGui::Spacing();
        }

        if (usesDlssd)
        {
            if (bool pOverride = config->DLSSDRenderPresetOverride.value_or_default();
                ImGui::Checkbox("渲染预设覆盖", &pOverride))
                config->DLSSDRenderPresetOverride = pOverride;

            ShowTooltip("每个渲染预设各有优劣\n"
                        "覆盖预设可能改善画质\n"
                        "启用/禁用后请点击应用");

            /*
            auto currentPresetIndex = GetPresetIndex(currentFeature, true);

            if (currentPresetIndex == 0)
                ImGui::Text("Current Preset: Default");
            else
                ImGui::Text("Current Preset: %c", 64 + currentPresetIndex);
            */

            ImGui::BeginDisabled(!config->DLSSDRenderPresetOverride.value_or_default() /*|| overridden*/);
            ImGui::PushItemWidth(135.0f * menuResScale);

            AddDLSSDRenderPreset("覆盖预设", &comboPreset);

            ImGui::PopItemWidth();
            ImGui::EndDisabled();
        }
        else
        {
            if (bool pOverride = config->RenderPresetOverride.value_or_default();
                ImGui::Checkbox("渲染预设覆盖", &pOverride))
                config->RenderPresetOverride = pOverride;

            ShowTooltip("每个渲染预设各有优劣\n"
                        "覆盖预设可能改善画质\n"
                        "启用/禁用后请点击应用");

            /*
            auto currentPresetIndex = GetPresetIndex(currentFeature, false);

            if (currentPresetIndex == 0)
                ImGui::Text("Current Preset: Default");
            else
                ImGui::Text("Current Preset: %c", 64 + currentPresetIndex);
            */

            ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() /*|| overridden*/);

            ImGui::PushItemWidth(135.0f * menuResScale);

            AddDLSSRenderPreset("覆盖预设", &comboPreset);

            ImGui::PopItemWidth();
            ImGui::EndDisabled();
        }

        ImGui::SameLine(0.0f, 6.0f);

        if (ImGui::Button("应用更改"))
        {
            LOG_DEBUG("正在应用 DLSS/DLSSD 预设覆盖，预设索引：{}", comboPreset.value_or_default());

            if (usesDlssd)
            {
                config->DLSSDRenderPresetForAll = comboPreset.value_or_default();

                // Ray reconstruction keeps its DLSSD, the upscalers their configured one
                state.newBackend = rayReconstruction ? Upscaler::Reset : Upscaler::DLSSD;
            }
            else
            {
                config->RenderPresetForAll = comboPreset.value_or_default();
                state.newBackend = SameBackend(currentBackend);
            }

            MARK_ALL_BACKENDS_CHANGED();
        }

        ImGui::Spacing();

        if (auto ch = ScopedCollapsingHeader(usesDlssd ? "高级 DLSS-D 设置" : "高级 DLSS 设置");
            ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            bool appIdOverride = config->UseGenericAppIdWithDlss.value_or_default();
            if (ImGui::Checkbox("对 DLSS 使用通用 App ID", &appIdOverride))
                config->UseGenericAppIdWithDlss = appIdOverride;

            ShowTooltip("对 NGX 使用通用 appid\n"
                        "修复 OptiScaler 预设覆盖在部分游戏中无效的问题\n"
                        "需要重启游戏");

            ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() || overridden);
            ImGui::Spacing();
            ImGui::PushItemWidth(135.0f * menuResScale);

            if (usesDlssd)
            {
                AddDLSSDRenderPreset("DLAA 预设", &config->DLSSDRenderPresetDLAA);
                AddDLSSDRenderPreset("极致质量预设", &config->DLSSDRenderPresetUltraQuality);
                AddDLSSDRenderPreset("质量预设", &config->DLSSDRenderPresetQuality);
                AddDLSSDRenderPreset("均衡预设", &config->DLSSDRenderPresetBalanced);
                AddDLSSDRenderPreset("性能预设", &config->DLSSDRenderPresetPerformance);
                AddDLSSDRenderPreset("极致性能预设", &config->DLSSDRenderPresetUltraPerformance);
            }
            else
            {
                AddDLSSRenderPreset("DLAA 预设", &config->RenderPresetDLAA);
                AddDLSSRenderPreset("极致质量预设", &config->RenderPresetUltraQuality);
                AddDLSSRenderPreset("质量预设", &config->RenderPresetQuality);
                AddDLSSRenderPreset("均衡预设", &config->RenderPresetBalanced);
                AddDLSSRenderPreset("性能预设", &config->RenderPresetPerformance);
                AddDLSSRenderPreset("极致性能预设", &config->RenderPresetUltraPerformance);
            }
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }
}

// Draws a line wherever an integer slider switches to the next step, so it's visible where to click
static void DrawSliderStepMarks(const ImVec2& framePos, float frameWidth, float frameHeight, int stepCount)
{
    if (stepCount < 2)
        return;

    constexpr float grabPadding = 2.0f; // Hardcoded in ImGui as well
    const float sliderSize = frameWidth - grabPadding * 2.0f;
    const float grabSize =
        std::min(std::max(sliderSize / (float) stepCount, ImGui::GetStyle().GrabMinSize), sliderSize);
    const float firstGrabCenter = framePos.x + grabPadding + grabSize * 0.5f;
    const float grabTravel = (sliderSize - grabSize) / (float) (stepCount - 1);
    const float inset = frameHeight * 0.2f;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text, 0.35f);
    auto drawList = ImGui::GetWindowDrawList();

    for (int step = 1; step < stepCount; step++)
    {
        // Snapped to the pixel grid to keep the 1 px line sharp, AddLine already centers it on the pixel
        const float x = std::floor(firstGrabCenter + grabTravel * ((float) step - 0.5f));
        drawList->AddLine(ImVec2(x, framePos.y + inset), ImVec2(x, framePos.y + frameHeight - inset), color);
    }
}

// Integer slider over [0, stepCount) with step marks and stepName(step) as the shown text
// Clicking a step applies it right away, while dragging only applies the step it's released on,
// so switching e.g. from Off to 4X doesn't go through every mode in between
// Returns true when step got changed
static bool RenderStepSlider(const char* label, int& step, int stepCount,
                             const std::function<std::string(int)>& stepName)
{
    // Step under the grab during a drag, -1 when not dragging
    auto storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetID(label);
    const int draggedStep = storage->GetInt(id, -1);
    int shownStep = draggedStep >= 0 ? draggedStep : step;

    const ImVec2 sliderPos = ImGui::GetCursorScreenPos();
    const float sliderWidth = ImGui::CalcItemWidth();
    const float sliderHeight = ImGui::GetFrameHeight();
    ImGui::SliderInt(label, &shownStep, 0, stepCount - 1, stepName(shownStep).c_str(), ImGuiSliderFlags_NoInput);
    DrawSliderStepMarks(sliderPos, sliderWidth, sliderHeight, stepCount);

    // The click itself moves the grab on the activation frame, that's the only change applied immediately
    const bool apply = (ImGui::IsItemActivated() || ImGui::IsItemDeactivated()) && shownStep != step;

    if (ImGui::IsItemActive())
        storage->SetInt(id, shownStep);
    else if (draggedStep >= 0)
        storage->SetInt(id, -1);

    if (!apply)
        return false;

    step = shownStep;
    return true;
}

// Slider for overriding the game's FG ratio: Off, Default (no override), 2X, 3X... up to maxInterpolationCount
static bool RenderRatioOverrideSlider(const char* label, CustomOptional<int, NoDefault>& setting,
                                      int maxInterpolationCount)
{
    // 0 = Off, 1 = Default, 2+ = ratio (interpolated frame count + 1)
    int index = 1;
    if (setting.has_value())
        index = setting.value() == 0 ? 0 : std::min(setting.value(), maxInterpolationCount) + 1;

    auto stepName = [](int step) -> std::string
    {
        return step == 0 ? "关" : step == 1 ? "默认" : std::format("{}X", step);
    };

    if (!RenderStepSlider(label, index, maxInterpolationCount + 2, stepName))
        return false;

    if (index == 1)
        setting.reset();
    else
        setting = index == 0 ? 0 : index - 1;

    return true;
}

void MenuCommon::RenderFrameGenerationSelection(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    /// FG INPUTS

    static std::vector<MenuOption<FGInput>> inputOptions;
    inputOptions.clear();

    // clang-format off

    inputOptions = {
        { FGInput::NoFG, "无" },
        { FGInput::Upscaler, "OptiFG（超分器）",
            "必须启用超分器\n\n可用于任何帧生成输出，但在某些情况下可能不完美\n为防止 UI 闪烁，需要 HUDfix" },
        { FGInput::DLSSG, "通过 Streamline 的 DLSSG",
            "可用于任何帧生成输出\n\n需要在游戏设置中启用 DLSS-FG\n开箱即用支持 HUDless\n\n仅限使用 Streamline 的游戏" },
        { FGInput::NvngxFG, "通过 Nvngx 的 DLSSG",
            "仅限 FSR FG 的变体\n\n需要在游戏设置中启用 DLSS-FG\n开箱即用支持 HUDless\n使用 Streamline 交换链进行节奏控制" },
        { FGInput::FSRFG, "FSR 3.1 帧生成",
            "可用于任何帧生成输出\n\n需要在游戏设置中启用 FSR-FG\n开箱即用支持 HUDless" },
        { FGInput::FSRFG30, "FSR 3.0 帧生成",
            "可用于任何帧生成输出\n\n需要在游戏设置中启用 FSR-FG\n开箱即用支持 HUDless" },
        { FGInput::XeFG, "XeFG",
            "可用于任意 FG 输出\n\n需要在游戏设置中启用 XeSS-FG\n开箱支持 HUDless" }
    };

    // clang-format on

    auto constexpr nvngxInputIndex = (uint32_t) FGInput::NvngxFG;

    // XeFG input requirements
    auto constexpr xefgInputIndex = (uint32_t) FGInput::XeFG;
    inputOptions[xefgInputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

    // OptiFG requirements
    auto constexpr optiFgIndex = (uint32_t) FGInput::Upscaler;
    // FSR FG's Vulkan dll is loaded with the device
    if (state.swapchainApi != API::Vulkan && !inputOptions[optiFgIndex].disabled &&
        state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFsrFgReady() && !ffxInitTried)
    {
        ffxInitTried = true;
        FfxApiProxy::InitFfxDx12();
        inputOptions[optiFgIndex].set_disabled(!FfxApiProxy::IsFsrFgReady(), "缺少 amd_fidelityfx_dx12.dll");
    }
    else if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::XeFG && !xefgInitTried &&
             XeFGProxy::Module() == nullptr)
    {
        xefgInitTried = true;
        XeFGProxy::InitXeFG();
        inputOptions[optiFgIndex].set_disabled(XeFGProxy::Module() == nullptr, "缺少 libxess_fg.dll");
    }

    // DLSSG inputs requirements
    auto constexpr dlssgInputIndex = (uint32_t) FGInput::DLSSG;
    // inputOptions[dlssgInputIndex].set_disabled(state.streamlineVersion.major == 0, "Game doesn't use streamline");
    inputOptions[dlssgInputIndex].set_disabled(state.swapchainApi == API::DX11, "不支持的 API");

    // FSRFG inputs requirements
    auto constexpr fsrfgInputIndex = (uint32_t) FGInput::FSRFG;
    inputOptions[fsrfgInputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

    // FSRFG30 inputs requirements
    auto constexpr fsrfg30InputIndex = (uint32_t) FGInput::FSRFG30;
    inputOptions[fsrfg30InputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

    if (!config->FGInput.has_value())
        config->FGInput = config->FGInput.value_or_default(); // need to have a value before combo

    /// FG OUTPUTS

    static std::vector<MenuOption<FGOutput>> outputOptions;
    outputOptions.clear();

    // clang-format off

    outputOptions = {
        { FGOutput::NoFG, "无" },
        { FGOutput::FSRFG, "FSR 帧生成", "FSR3/4-FG，RDNA4 会自动升级到 FSR4-FG\n\nFSR4-FG 有时比 XeFG 更好/更差" },
        { FGOutput::DLSSG, "DLSSG", "DLSSG 输出\n例如可以与 Nukem 的方案配合使用" },
        { FGOutput::XeFG, "XeFG", "XeFG - 开销最大，但通用性最好的帧生成\n\nXeFG 3 整体上对 HUD 处理最好\n\n如果 HUD 有拖影请启用 UI 合成" },
        { FGOutput::Reprojection, "重投影（WIP）", "使用新的鼠标数据重投影游戏画面\n类似 Reflex 2，可降低感知延迟\n\n- 需要以 Streamline 的 DLSSG 作为输入\n- 仅适用于第一人称视角游戏\n- 仅支持鼠标，不支持手柄\n- 如有可能，请禁用游戏中的鼠标/相机平滑\n" },
        { FGOutput::FlipMeter, "翻转节流（WIP）", "由 AMD 驱动翻转节流来节拍的 FG Nvngx 替换帧\n\n- 仅限 AMD，需要驱动支持\n- 仅在独占或无边框全屏下生成\n- 支持 Anti-Lag 2" },
    };

    // clang-format on

    // DLSSG output requirements
    auto constexpr dlssgOutputIndex = (uint32_t) FGOutput::DLSSG;
    const bool maySupportDlssg = primaryGpu.vendorId == VendorId::Nvidia;
    const bool hasDlssgReplacement = state.nukemsFgFileAvailable || state.artursFgFileAvailable ||
                                     FfxApiProxy::IsFGReady(false) ||
                                     !CustomProviders::List(CustomProviders::Kind::Dlssg).empty();

    if (!maySupportDlssg && hasDlssgReplacement)
    {
        outputOptions[dlssgOutputIndex].tooltip =
            "无真正的 DLSSG，硬件不受支持\n仅可用 Nvngx 帧生成替换";
    }

    outputOptions[dlssgOutputIndex].set_disabled(!maySupportDlssg && !hasDlssgReplacement,
                                                 "硬件不受支持且没有可用的替换方案");

    // For that one case of DX11 DLSSG
    const auto streamlineVersion = state.streamlineVersion;
    const bool nukemsUnsupportedApi =
        state.swapchainApi == API::DX11 &&
        (streamlineVersion == feature_version { 0, 0, 0 } || streamlineVersion > feature_version { 2, 0, 1 });
    inputOptions[nvngxInputIndex].set_disabled(nukemsUnsupportedApi, "不支持的 API");

    // XeFG output requirements
    auto constexpr xefgOutputIndex = (uint32_t) FGOutput::XeFG;
    outputOptions[xefgOutputIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");

    // Flip meter output requirements, the driver is only asked once its swapchain gets created
    auto constexpr flipMeterOutputIndex = (uint32_t) FGOutput::FlipMeter;
    outputOptions[flipMeterOutputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");
    outputOptions[flipMeterOutputIndex].set_disabled(primaryGpu.vendorId != VendorId::AMD, "不支持的硬件");
    outputOptions[flipMeterOutputIndex].set_disabled(!state.amdFlipMeterSupported.value_or(true),
                                                     "驱动不支持翻转节流");
    // Unsupported FG input selected
    const auto currentInputIndex = (uint32_t) state.activeFgInput;
    if (config->FGInput != FGInput::NoFG && inputOptions.size() > currentInputIndex &&
        inputOptions[currentInputIndex].disabled && state.activeFgInput == config->FGInput)
    {
        LOG_WARN("正在将 FGInput 重置为 NoFG：{}", inputOptions[currentInputIndex].label);
        config->FGInput = FGInput::NoFG;

        // Changing active can be dangerous but we are talking about an unsupported mode
        // which shouldn't even actually have taken affect
        state.activeFgInput = FGInput::NoFG;
    }

    // Unsupported FG output selected
    const auto currentOutputIndex = (uint32_t) state.activeFgOutput;
    if (config->FGOutput != FGOutput::NoFG && outputOptions.size() > currentOutputIndex &&
        outputOptions[currentOutputIndex].disabled && state.activeFgOutput == config->FGOutput)
    {
        LOG_WARN("正在将 FGOutput 重置为 NoFG：{}", outputOptions[currentOutputIndex].label);
        config->FGOutput = FGOutput::NoFG;
        state.activeFgOutput = FGOutput::NoFG;
    }

    if (!config->FGOutput.has_value())
        config->FGOutput = config->FGOutput.value_or_default(); // need to have a value before combo

    /// FG NVNGX REPLACEMENT

    static std::vector<MenuOption<FGNvngxReplacement>> nvngxOptions;
    nvngxOptions.clear();

    // clang-format off

    nvngxOptions = {
        { FGNvngxReplacement::None, "无（真正的 DLSSG）", "真正的 DLSSG，适用于 RTX 40 系列及以上"},
        { FGNvngxReplacement::Nukems, "Nukem 的", "FSR 3 帧生成" },
        { FGNvngxReplacement::Arturs, "Enabler", "FSR 3 MFG 模组" },
        { FGNvngxReplacement::FFX, "FSR 3/4 帧生成", "使用 FFX 升级的 FSR 3/4 帧生成\n\n"
                                                 "部分基于 Nukems，使用 SL 交换链\n"
                                                 "与 FSR-FG 输出相比可能有更好的性能和帧节奏"},
        { FGNvngxReplacement::Combo, "FFX + Enabler", "如果支持 FSR4-FG 就使用它，否则沿用 Enabler\n\n"
                                                      "中间假帧使用 FFX，其余使用 Enabler\n\n"
                                                      "2x - FFX\n3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler\n\n"
                                                      "由于节奏限制，只有奇数个假帧才能使用 FFX"},
    };

    // clang-format on

    // Custom ones go after the rest, each one is a value from FGNvngxReplacement::Custom on
    const auto customFgs = CustomProviders::List(CustomProviders::Kind::Dlssg);
    auto constexpr fgNvngxCustomIndex = (uint32_t) FGNvngxReplacement::Custom;

    for (uint32_t i = 0; i < customFgs.size(); i++)
        nvngxOptions.push_back({ (FGNvngxReplacement) (fgNvngxCustomIndex + i), customFgs[i],
                                 StrFmt("Custom DLSSG from custom\\%s\\dlssg", customFgs[i].c_str()) });

    bool replaceFgOutputWithNvngx = false;
    bool showNvngxFgDowndown = false;

    if (config->FGInput == FGInput::NvngxFG)
    {
        config->FGOutput = FGOutput::NoFG;
        replaceFgOutputWithNvngx = true;
    }
    else if (config->FGOutput == FGOutput::DLSSG || config->FGOutput == FGOutput::FlipMeter)
    {
        showNvngxFgDowndown = true;
    }

    auto constexpr fgNvngxNoneIndex = (uint32_t) FGNvngxReplacement::None;
    nvngxOptions[fgNvngxNoneIndex].set_disabled(!maySupportDlssg, "不支持的硬件");

    if (replaceFgOutputWithNvngx)
    {
        nvngxOptions[fgNvngxNoneIndex].label = "无";
        nvngxOptions[fgNvngxNoneIndex].set_hidden(true);
    }
    else if (config->FGOutput == FGOutput::FlipMeter)
    {
        // The flip meter output has no frames of its own
        nvngxOptions[fgNvngxNoneIndex].set_hidden(true);
    }

    auto constexpr fgNvngxNukemsIndex = (uint32_t) FGNvngxReplacement::Nukems;
    nvngxOptions[fgNvngxNukemsIndex].set_disabled(!state.nukemsFgFileAvailable,
                                                  "缺少 dlssg_to_fsr3_amd_is_better.dll");

    auto constexpr fgNvngxArtursIndex = (uint32_t) FGNvngxReplacement::Arturs;
    nvngxOptions[fgNvngxArtursIndex].set_disabled(!state.artursFgFileAvailable, "缺少 dlss-enabler-headless.dll");

    auto constexpr fgNvngxFfxIndex = (uint32_t) FGNvngxReplacement::FFX;
    if (state.swapchainApi == API::Vulkan)
        nvngxOptions[fgNvngxFfxIndex].set_disabled(!FfxApiProxy::IsFGReadyVk() && !FSR4DriverProviderVk::InitFg(),
                                                   "Missing amd_fidelityfx_framegeneration_vk.dll");
    else
        nvngxOptions[fgNvngxFfxIndex].set_disabled(!FfxApiProxy::IsFGReady(false),
                                                   "缺少 amd_fidelityfx_framegeneration_dx12.dll");

    auto constexpr fgNvngxComboIndex = (uint32_t) FGNvngxReplacement::Combo;
    nvngxOptions[fgNvngxComboIndex].set_disabled(state.swapchainApi == API::Vulkan, "不支持的 API");
    nvngxOptions[fgNvngxComboIndex].set_disabled(
        !FfxApiProxy::IsFGReady(false) || !state.artursFgFileAvailable,
        "缺少 amd_fidelityfx_framegeneration_dx12.dll\n或缺少 dlss-enabler-headless.dll");

    // TODO: Automatically switch to any other option

    if (!config->FGNvngxReplacement.has_value())
        config->FGNvngxReplacement = config->FGNvngxReplacement.value_or_default(); // need to have a value before combo

    // Folder names in FGNvngxReplacement aren't case sensitive
    auto isActiveCustomFg = [&config, &state](const std::string& name)
    {
        return config->FGNvngxReplacement.value_or_default() == FGNvngxReplacement::Custom &&
               _stricmp(state.fgNvngxCustom.c_str(), name.c_str()) == 0;
    };

    auto nvngxCombo = [&](const char* label)
    {
        auto current = config->FGNvngxReplacement.value_or_default();

        if (current == FGNvngxReplacement::Custom)
            current = (FGNvngxReplacement) (fgNvngxCustomIndex +
                                            (std::ranges::find_if(customFgs, isActiveCustomFg) - customFgs.begin()));

        auto selected = current;
        PopulateCombo(label, selected, nvngxOptions);

        if (selected == current)
            return;

        if ((uint32_t) selected >= fgNvngxCustomIndex)
        {
            state.fgNvngxCustom = customFgs[(uint32_t) selected - fgNvngxCustomIndex];
            config->FGNvngxReplacement = FGNvngxReplacement::Custom;
        }
        else
        {
            state.fgNvngxCustom.clear();
            config->FGNvngxReplacement = selected;
        }
    };

    if (state.activeFgInput != FGInput::ForceXeLL)
    {
        SectionTitle("帧生成");
        ShowTooltip("你要选哪种安慰剂？");

        if (ImGui::BeginTable("fgSelection", 1, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();

            PopulateCombo("帧生成输入", config->FGInput, inputOptions);
            ShowTooltip("用于帧生成的数据源\n"
                        "游戏支持的原生帧生成");

            ImGui::TableNextColumn();

            if (replaceFgOutputWithNvngx)
            {
                // Disable None?
                nvngxCombo("FG Nvngx");
                ShowTooltip("用来替代真正 DLSSG 的后端");
            }
            else
            {
                PopulateCombo("帧生成输出", config->FGOutput, outputOptions);
                ShowTooltip("您实际将使用的帧生成");
            }

            if (showNvngxFgDowndown)
            {
                ImGui::TableNextColumn();

                nvngxCombo("FG Nvngx 替换");
                ShowTooltip(config->FGOutput == FGOutput::FlipMeter ? "由哪个后端生成帧"
                                                                    : "用来替代真正 DLSSG 的后端");
            }

            // Custom FFX FGs of the swapchain API, DX11 runs the DX12 one
            const auto customFsrFgs = CustomProviders::List(
                state.swapchainApi == API::Vulkan ? CustomProviders::Kind::FfxFgVk : CustomProviders::Kind::FfxFg);

            if (config->FGOutput == FGOutput::FSRFG && !customFsrFgs.empty())
            {
                ImGui::TableNextColumn();

                auto provider = config->FSRFGProvider.value_or("");

                if (ImGui::BeginCombo("FSR FG 提供者", provider.empty() ? "默认" : provider.c_str()))
                {
                    if (ImGui::Selectable("默认", provider.empty()))
                        config->FSRFGProvider.reset();

                    for (auto& name : customFsrFgs)
                    {
                        if (ImGui::Selectable(name.c_str(), _stricmp(provider.c_str(), name.c_str()) == 0))
                            config->FSRFGProvider = name;
                    }

                    ImGui::EndCombo();
                }

                ShowTooltip("FSR FG 使用的 FFX 帧生成与交换链\n"
                            "Custom ones are from custom\\<name>\\ffx_fg(_vk) or plugins");
            }

            ImGui::EndTable();
        }

        // Try to avoid having None selected when the gpu doesn't support DLSSG + some fallbacks
        if (!maySupportDlssg && (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
            config->FGNvngxReplacement.value_or_default() == FGNvngxReplacement::None)
        {
            if (state.nukemsFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Nukems);

            else if (state.artursFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Arturs);

            else if (FfxApiProxy::IsFGReady(false))
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::FFX);

            else if (!customFgs.empty())
            {
                state.fgNvngxCustom = customFgs[0];
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Custom);
            }
        }

        const bool nvngxFgChanged =
            (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
            (state.activeFgNvngx != config->FGNvngxReplacement.value_or_default() ||
             (state.activeFgNvngx == FGNvngxReplacement::Custom && !isActiveCustomFg(state.activeFgNvngxCustom)));
        // XeFG input to XeFG output runs without OptiScaler's FG output
        auto configFgOutput = config->FGOutput.value_or_default();

        if (config->FGInput.value_or_default() == FGInput::XeFG && configFgOutput == FGOutput::XeFG)
            configFgOutput = FGOutput::NoFG;

        state.fgSettingsChanged =
            state.activeFgOutput != configFgOutput || state.activeFgInput != config->FGInput.value_or_default() ||
            nvngxFgChanged ||
            _stricmp(state.activeFsrFgProvider.c_str(), config->FSRFGProvider.value_or("").c_str()) != 0;

        if (state.fgSettingsChanged)
        {
            ImGui::Spacing();
            ImGui::TextColored(StatusError(), "保存设置并重启游戏以应用更改");
            ImGui::Spacing();
        }

        // XeFG input to XeFG output: the game's own XeFG, like Override DLSSG Ratio
        if (XeFGInputs::Passthrough() && XeFGInputs::MaxInterpolations() >= 1)
        {
            ImGui::PushItemWidth(150.0f * menuResScale);
            RenderRatioOverrideSlider("覆盖 XeFG 倍率", config->FGXeFGOverrideInterpolationCount,
                                      (int) XeFGInputs::MaxInterpolations());
            ImGui::PopItemWidth();
        }

        const bool dlssgInputOrOutput =
            state.activeFgOutput == FGOutput::DLSSG || state.activeFgInput == FGInput::DLSSG;

        ImGui::BeginDisabled(state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default());
        if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1 && !dlssgInputOrOutput)
        {
            ImGui::PushItemWidth(150.0f * menuResScale);

            if (RenderRatioOverrideSlider("覆盖 DLSSG 倍率", config->FGDLSSGOverrideInterpolationCount,
                                          state.dlssgMfgMax.value()))
            {
                LOG_DEBUG("DLSSG 插帧数设为：{}",
                          config->FGDLSSGOverrideInterpolationCount.has_value()
                              ? std::to_string(config->FGDLSSGOverrideInterpolationCount.value())
                              : std::string("默认"));

                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::PopItemWidth();
        }

        ImGui::EndDisabled();

        if (state.dlssgGameDMFGSupported && !dlssgInputOrOutput)
        {
            ImGui::SameLine(0.0f, 16.0f);

            if (bool dynamicMFG = config->FGDLSSGOverrideForceDMFG.value_or_default();
                ImGui::Checkbox("强制动态 MFG", &dynamicMFG))
            {
                config->FGDLSSGOverrideForceDMFG = dynamicMFG;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::BeginDisabled(state.dlssgLastSetMode != sl::DLSSGMode::eDynamic);
            static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
            const float fpsSliderMax = (float) std::max(200, refreshRate);
            ImGui::SliderFloat("DMFG FPS 目标", &fpsTarget, 0, fpsSliderMax, "%.0f");

            ShowTooltip("限制值为 0 表示自动检测显示器刷新率");

            if (ImGui::Button("应用目标"))
            {
                config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("重置目标"))
            {
                fpsTarget = 0.0f;
                config->FGDLSSGFramerateTargetDMFG.reset();
            }

            ImGui::EndDisabled();
        }

        auto fgOutput = state.CurrentFG();
        if ((state.activeFgOutput != FGOutput::Reprojection && state.activeFgOutput != FGOutput::NoFG &&
             state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::NvngxFG) &&
            fgOutput)
        {
            ImGui::Checkbox("显示检测到的 UI", &state.fgHudlessCompare);
            ShowTooltip("需要 HUDless 纹理与最终画面进行比较。\n"
                        "UI 元素（且仅 UI 元素）应呈现粉色色调！");

            const auto isUsingUIAny = fgOutput->IsUsingUIAny();

            ImGui::BeginDisabled(!isUsingUIAny);

            if (bool drawUIOverFG = config->FGDrawUIOverFG.value_or_default();
                ImGui::Checkbox("在最终画面上绘制 UI", &drawUIOverFG))
            {
                config->FGDrawUIOverFG = drawUIOverFG;
            }
            ShowTooltip("将 UI 资源绘制到最终画面上\n"
                        "如果看不到 UI，请启用此项！");

            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!isUsingUIAny || !config->FGDrawUIOverFG.value_or_default());

            if (bool uiPremultipliedAlpha = config->FGUIPremultipliedAlpha.value_or_default();
                ImGui::Checkbox("UI 预乘 Alpha", &uiPremultipliedAlpha))
            {
                config->FGUIPremultipliedAlpha = uiPremultipliedAlpha;
            }
            ShowTooltip("若 UI 过亮，请禁用此选项");

            ImGui::EndDisabled();
        }

        const bool showOutputSpecificFGSettings =
            state.activeFgInput == FGInput::DLSSG || state.activeFgInput == FGInput::FSRFG ||
            state.activeFgInput == FGInput::FSRFG30 || state.activeFgInput == FGInput::XeFG;

        const bool showHudCutoff = state.activeFgInput == FGInput::NvngxFG || state.activeFgOutput == FGOutput::FSRFG;

        if (showOutputSpecificFGSettings || showHudCutoff)
        {
            if (auto ch = ScopedCollapsingHeader("高级帧生成设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (showOutputSpecificFGSettings)
                {
                    auto fgOutput = state.CurrentFG();
                    if (fgOutput)
                    {
                        ImGui::BeginDisabled(!fgOutput->IsActive());

                        const auto isUsingUIAny = fgOutput->IsUsingUIAny();
                        const auto isUsingHudlessAny = fgOutput->IsUsingHudlessAny();

                        bool disableUI = config->FGDisableUI.value_or_default();
                        ImGui::BeginDisabled(!isUsingUIAny && !disableUI);

                        if (ImGui::Checkbox("禁用 UI 纹理", &disableUI))
                        {
                            config->FGDisableUI = disableUI;
                            fgOutput->UpdateTarget();
                        }

                        ShowTooltip("当游戏发送 UI 纹理但您想禁用它时");

                        ImGui::EndDisabled();

                        ImGui::SameLine(0.0f, 16.0f);

                        bool disableHudless = config->FGDisableHudless.value_or_default();
                        ImGui::BeginDisabled(!isUsingHudlessAny && !disableHudless);

                        if (ImGui::Checkbox("禁用 HUDless", &disableHudless))
                        {
                            config->FGDisableHudless = disableHudless;
                        }

                        ShowTooltip("当游戏发送 HUDless 但您想禁用它时");

                        ImGui::EndDisabled();

                        bool depthValidNow = config->FGDepthValidNow.value_or_default();
                        if (ImGui::Checkbox("将深度作为 ValidNow", &depthValidNow))
                            config->FGDepthValidNow = depthValidNow;

                        ShowTooltip("会占用更多显存，但 Uniscaler 需要此项\n"
                                    "也许其他一些游戏也需要");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool velocityValidNow = config->FGVelocityValidNow.value_or_default();
                        if (ImGui::Checkbox("将运动矢量作为 ValidNow", &velocityValidNow))
                            config->FGVelocityValidNow = velocityValidNow;

                        ShowTooltip("会占用更多显存，但 Uniscaler 需要此项\n"
                                    "也许其他一些游戏也需要");

                        bool hudlessValidNow = config->FGHudlessValidNow.value_or_default();
                        if (ImGui::Checkbox("将 HUDless 作为 ValidNow", &hudlessValidNow))
                            config->FGHudlessValidNow = hudlessValidNow;

                        ShowTooltip("会占用更多显存，但某些游戏可能需要此项");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool firstHudless = config->FGOnlyAcceptFirstHudless.value_or_default();
                        if (ImGui::Checkbox("接受首个 HUDless", &firstHudless))
                            config->FGOnlyAcceptFirstHudless = firstHudless;

                        ShowTooltip("如果源标记了多个 HUDless，仅使用第一个");

                        if (bool skipReset = config->FGSkipReset.value_or_default();
                            ImGui::Checkbox("跳过重置", &skipReset))
                        {
                            config->FGSkipReset = skipReset;
                        }

                        ShowTooltip("不使用帧生成输入的复位信号");

                        ImGui::EndDisabled();

                        ImGui::PushItemWidth(80.0f * menuResScale);

                        auto frameAhead = config->FGAllowedFrameAhead.value_or_default();
                        if (ImGui::InputInt("领先帧数", &frameAhead, 1, 1) && frameAhead > 0 && frameAhead < 4)
                        {
                            config->FGAllowedFrameAhead = frameAhead;
                        }

                        ShowTooltip("帧生成允许领先于游戏的帧数\n"
                                    "可能阻止帧生成的开关切换，但也可能导致问题");

                        ImGui::PopItemWidth();

                        ImGui::SameLine(0.0f, 16.0f);

                        const char* ftSources[] = { "输入", "Opti", "归零" };
                        const char* ftSourceInfos[] = { "使用 DLSSG 或 FSR-FG 提供的帧时间",
                                                        "使用 Opti 计算的帧时间",
                                                        "让 XeFG 处理帧时间" };

                        auto currentSet = (int) config->FTInput.value_or_default();
                        auto currentSourceCount = state.activeFgOutput == FGOutput::XeFG ? 3 : 2;

                        ImGui::PushItemWidth(95.0f * menuResScale);

                        if (ImGui::BeginCombo("帧时间输入", ftSources[currentSet]))
                        {
                            for (size_t i = 0; i < currentSourceCount; i++)
                            {

                                if (ImGui::Selectable(ftSources[i], currentSet == i))
                                {
                                    LOG_DEBUG("FTInput 已更改 {} -> {}", ftSources[currentSet], ftSources[i]);
                                    config->FTInput = (FrameTimeSource) i;
                                }

                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                    ImGui::SetTooltip(ftSourceInfos[i]);
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::PopItemWidth();

                        ShowTooltip("选择帧时间来源\n"
                                    "可能有助于帧节奏和卡顿问题");
                    }
                }

                if (showHudCutoff)
                {
                    float fgHudCutoff = config->FGHudCutoff.value_or_default();
                    if (ImGui::SliderFloat("HUD 裁切", &fgHudCutoff, 0.00f, 1.0f, "%.2f"))
                        config->FGHudCutoff = fgHudCutoff;

                    ShowTooltip("从 UI 中裁掉透明区域以帮助插帧\n"
                                "可以使用“显示检测到的 UI”来查看差异\n0.0 表示自动");
                }
            }
        }
    }
}

// Replaces the Active checkbox + MFG combo of FG outputs with a single slider
// maxInterpolationCount of 0 means the output doesn't generate frames, "On" is shown instead of "2X"
void MenuCommon::RenderFGModeSlider(RenderMenuContext& ctx, const char* label, int maxInterpolationCount,
                                    CustomOptional<int>* countConfig, const char* tooltip)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // -1 = Default (follow game), 0 = Off, >0 = interpolated frame count
    auto modeName = [&](int mode) -> std::string
    {
        if (mode < 0)
            return "默认";

        if (mode == 0)
            return "关";

        if (maxInterpolationCount == 0)
            return "开";

        return std::format("{}X", mode + 1);
    };

    const int maxMode = std::max(maxInterpolationCount, 1);

    int current = 0;
    if (config->FGFollowsGame())
        current = -1;
    else if (config->FGEnabled.value_or_default())
        current = countConfig != nullptr ? std::clamp(countConfig->value_or_default(), 1, maxMode) : 1;

    // Slider order: Off, (Default), 2X, 3X... up to the output's max
    std::vector<int> modes { 0 };
    if (Config::FGCanFollowGame())
        modes.push_back(-1);

    for (int mode = 1; mode <= maxMode; mode++)
        modes.push_back(mode);

    int index = (int) (std::find(modes.begin(), modes.end(), current) - modes.begin());
    if (index >= (int) modes.size())
        index = 0;

    auto stepName = [&](int step) -> std::string
    {
        const int mode = modes[step];
        if (mode >= 0)
            return modeName(mode);

        const int gameMode =
            state.dlssgInputGameEnabled ? std::min(state.dlssgInputGameInterpolationCount, maxMode) : 0;
        return std::format("{} ({})", modeName(mode), modeName(gameMode));
    };

    ImGui::PushItemWidth(150.0f * ctx.menuResScale);

    if (RenderStepSlider(label, index, (int) modes.size(), stepName) && modes[index] != current)
    {
        const int mode = modes[index];
        fgHotkeyLeftDefault = false;

        if (mode < 0)
        {
            config->FGEnabled = std::optional<bool> {};
        }
        else
        {
            config->FGEnabled = mode > 0;

            if (mode > 0 && countConfig != nullptr)
                *countConfig = mode;
        }

        LOG_DEBUG("FG mode set to: {}", modeName(mode));

        if (config->IsFGEnabled())
            state.fgChanged = true;
    }

    ImGui::PopItemWidth();

    if (tooltip != nullptr)
        ShowTooltip(tooltip);
    else if (Config::FGCanFollowGame())
        ShowTooltip("帧生成模式\n默认跟随游戏的 DLSS FG 设置");
    else
        ShowTooltip("帧生成模式");
}

void MenuCommon::RenderFrameGenerationRuntimeSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;
    auto fgOutput = state.currentFG;

    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool outputIsHdr = uiTargetMode != UiTargetMode::SDR;

    // FSR FG controls
    if (ScopedCard::Shows("fg_fsrfg") && state.activeFgOutput == FGOutput::FSRFG &&
        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists())
    {
        if (state.activeFgInput != FGInput::Upscaler ||
            (currentFeature != nullptr && !currentFeature->IsFrozen()) &&
                (state.currentVkFG != nullptr || FfxApiProxy::IsFsrFgReady()))
        {
            ScopedCard card { "FSR FG 设置" };

            // The output resets an index from a game with more FFX versions
            if (_ffxFGIndex < 0 || _ffxFGIndex >= (int) state.ffxFGVersionNames.size())
                _ffxFGIndex = config->FfxFGIndex.value_or_default();

            if (state.ffxFGVersionNames.size() > 0)
            {
                ImGui::PushItemWidth(115.0f * menuResScale);

                auto currentName = StrFmt("FSR %s", state.ffxFGVersionNames[_ffxFGIndex]);
                if (ImGui::BeginCombo("FFX FG 版本", currentName.c_str()))
                {
                    for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                    {
                        auto name = StrFmt("FSR %s", state.ffxFGVersionNames[n]);
                        if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                        {
                            _ffxFGIndex = n;
                            config->FfxFGIndex = _ffxFGIndex;
                            state.fgChanged = true;
                            state.scChanged = true;
                        }
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ShowTooltip("FFX SDK 报告的帧生成器列表");
            }

            RenderFGModeSlider(ctx, "帧生成##fsrfg", 1, nullptr);

            bool fgAsync = config->FGAsync.value_or_default();
            if (ImGui::Checkbox("允许异步", &fgAsync))
            {
                config->FGAsync = fgAsync;

                if (config->IsFGEnabled())
                {
                    state.fgChanged = true;
                    state.scChanged = true;
                    LOG_DEBUG("异步已设置 FGChanged");
                }
            }
            ShowTooltip("启用异步可获得更好的帧生成性能\n可能导致崩溃，尤其在使用 HUD 修复时！");

            if (state.amdFlipMeterCreated)
            {
                ImGui::SameLine(0.0f, 16.0f);

                bool disableFlipMetering = config->FSRFGDisableFlipMetering.value_or_default();
                if (ImGui::Checkbox("禁用硬件翻转节流", &disableFlipMetering))
                {
                    config->FSRFGDisableFlipMetering = disableFlipMetering;
                    Amdxc64Hooks::ApplyFlipMeterOverride();
                }
                ShowTooltip("覆盖 FSR 请求的 AMD 硬件翻转节流");

                ImGui::SameLine(0.0f, 16.0f);

                if (state.amdFlipMeterMode != 0)
                    ImGui::TextDisabled("（模式 %u）", state.amdFlipMeterMode);
                else
                    ImGui::TextDisabled("（关）");

                ShowTooltip("驱动中应用的翻转节流模式\nFSR 仅在独占或 "
                            "无边框全屏");
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("高级 FSR FG 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (ImGui::BeginTable("fsrfgAdvanced", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ConfigCheckbox("调试视图##2", config->FGDebugView,
                                       "启用 FSR3.1-FG 调试视图\n\n"
                                       "左上：游戏运动矢量\n"
                                       "中上：GMV 深度\n"
                                       "右上：光流运动矢量\n"
                                       "中间：仅插值帧\n"
                                       "左下：去遮挡蒙版\n"
                                       "中下：插值来源（无 UI）\n"
                                       "右下：HUDless 资源") &&
                        config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("调试视图已设置 FGChanged");
                    }

                    if (state.currentFG && state.currentFG->Version().major > 3)
                    {
                        ImGui::TableNextColumn();
                        if (ConfigCheckbox("启用水印", config->FSRFGEnableWatermark,
                                           "启用 FSR FG 水印"))
                        {
                            Util::SetFlagEnvVar(L"MLFI-WATERMARK", config->FSRFGEnableWatermark.value_or_default());

                            // The driver reads the variable on context creation, SCchanged also required
                            if (config->IsFGEnabled())
                            {
                                state.fgChanged = true;
                                state.scChanged = true;
                            }
                        }
                    }

                    ImGui::TableNextColumn();
                    ImGui::Checkbox("仅帧生成的帧", &state.fgOnlyGenerated);
                    ShowTooltip("仅显示 FSR 3.1 生成的帧");

                    ImGui::TableNextColumn();
                    ConfigCheckbox("调试重置线", config->FGDebugResetLines,
                                   "启用绘制插值跳过线");

                    ImGui::TableNextColumn();
                    ConfigCheckbox("调试撕裂线", config->FGDebugTearLines,
                                   "启用绘制撕裂与插值跳过线");

                    ImGui::TableNextColumn();
                    ConfigCheckbox("调试节奏线", config->FGDebugPacingLines, "启用绘制节奏线");

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::TreeNode("帧生成区域设置"))
                {
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    int rectLeft = config->FGRectLeft.value_or(0);
                    if (ImGui::InputInt("区域左", &rectLeft))
                        config->FGRectLeft = rectLeft;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectTop = config->FGRectTop.value_or(0);
                    if (ImGui::InputInt("区域上", &rectTop))
                        config->FGRectTop = rectTop;

                    int rectWidth = config->FGRectWidth.value_or(0);
                    if (ImGui::InputInt("区域宽", &rectWidth))
                        config->FGRectWidth = rectWidth;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectHeight = config->FGRectHeight.value_or(0);
                    if (ImGui::InputInt("区域高", &rectHeight))
                        config->FGRectHeight = rectHeight;

                    ImGui::PopItemWidth();
                    ShowTooltip("帧生成区域，为黑边内容调整");

                    ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                         !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                    if (ImGui::Button("重置帧生成区域"))
                    {
                        config->FGRectLeft.reset();
                        config->FGRectTop.reset();
                        config->FGRectWidth.reset();
                        config->FGRectHeight.reset();
                    }

                    ShowTooltip("重置帧生成区域");

                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                // Vulkan FSR-FG reports FFX SDK versions, SDK 1.1.3 is FSR 3.1.3
                auto fg = state.CurrentFG();
                auto fptMinVersion =
                    state.currentVkFG != nullptr ? feature_version { 1, 1, 3 } : feature_version { 3, 1, 3 };
                if (fg != nullptr && strcmp(fg->Name(), "FSR-FG") == 0 && fg->Version() >= fptMinVersion)
                {
                    ImGui::Spacing();

                    if (ImGui::TreeNode("帧节奏调优"))
                    {
                        auto fptEnabled = config->FGFramePacingTuning.value_or_default();
                        if (ImGui::Checkbox("启用调优", &fptEnabled))
                        {
                            config->FGFramePacingTuning = fptEnabled;
                            state.fsrfgFramePaceTuningChanged = true;
                        }

                        ImGui::BeginDisabled(!config->FGFramePacingTuning.value_or_default());

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptSafetyMargin = config->FGFPTSafetyMarginInMs.value_or_default();
                        if (ImGui::InputFloat("安全余量（毫秒）", &fptSafetyMargin, 0.01f, 0.1f, "%.2f"))
                            config->FGFPTSafetyMarginInMs = fptSafetyMargin;
                        ShowTooltip("安全余量（毫秒）\n"
                                    "FSR 默认值：0.1ms\n"
                                    "Opti 默认值：0.01ms");

                        auto fptVarianceFactor = config->FGFPTVarianceFactor.value_or_default();
                        if (ImGui::SliderFloat("方差因子", &fptVarianceFactor, 0.0f, 1.0f, "%.2f"))
                            config->FGFPTVarianceFactor = fptVarianceFactor;
                        ShowTooltip("方差因子\n"
                                    "FSR 默认值：0.1\n"
                                    "Opti 默认值：0.3");
                        ImGui::PopItemWidth();

                        // FFX's Vulkan swapchain only reads the safety margin and variance factor
                        ImGui::BeginDisabled(state.currentVkFG != nullptr);

                        auto fpHybridSpin = config->FGFPTAllowHybridSpin.value_or_default();
                        if (ImGui::Checkbox("启用混合自旋", &fpHybridSpin))
                            config->FGFPTAllowHybridSpin = fpHybridSpin;
                        ShowTooltip("允许节奏自旋锁休眠，可降低 CPU 占用\n"
                                    "可能导致 FPS 爬升缓慢");

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptHybridSpinTime = config->FGFPTHybridSpinTime.value_or_default();
                        if (ImGui::SliderInt("混合自旋时间", &fptHybridSpinTime, 0, 100))
                            config->FGFPTHybridSpinTime = fptHybridSpinTime;
                        ShowTooltip("当 FPTHybridSpin 为 true 时自旋的时间。以计时器"
                                    "分辨率单位计量。\n"
                                    "不建议低于 2，否则会导致频繁过冲");
                        ImGui::PopItemWidth();

                        auto fpWaitForSingleObjectOnFence =
                            config->FGFPTAllowWaitForSingleObjectOnFence.value_or_default();
                        if (ImGui::Checkbox("启用 WaitForSingleObjectOnFence", &fpWaitForSingleObjectOnFence))
                        {
                            config->FGFPTAllowWaitForSingleObjectOnFence = fpWaitForSingleObjectOnFence;
                        }
                        ShowTooltip("允许使用 WaitForSingleObject 代替自旋等待围栏值");

                        ImGui::EndDisabled();

                        if (ImGui::Button("应用时序更改"))
                            state.fsrfgFramePaceTuningChanged = true;

                        ImGui::EndDisabled();
                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }

    // XeFG controls
    if (ScopedCard::Shows("fg_xefg") && state.activeFgOutput == FGOutput::XeFG &&
        state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::ForceXeLL &&
        state.currentFGSwapchain != nullptr && XeFGProxy::InitXeFG() && fgOutput)
    {
        ScopedCard card { "XeFG 设置" };

        bool ignoreChecks = config->FGXeFGIgnoreInitChecks.value_or_default();

        bool nativeAA = false;
        if (state.activeFgInput == FGInput::Upscaler && currentFeature != nullptr)
            nativeAA = currentFeature->RenderWidth() == currentFeature->DisplayWidth();

        const bool correctMVs = fgOutput->IsLowResMV() || nativeAA ||
                                (State::Instance().gameQuirks & GameQuirk::ForceFGRenderSizeMVs) || ignoreChecks;

        if (!correctMVs || state.realExclusiveFullscreen)
        {
            config->FGEnabled.set_volatile_value(false);
            config->FGXeFGDebugView.reset();
        }

        const bool restartNeeded = config->FGXeFGDepthInverted.value_or_default() != fgOutput->IsInvertedDepth() ||
                                   config->FGXeFGJitteredMV.value_or_default() != fgOutput->IsJitteredMVs() ||
                                   config->FGXeFGHighResMV.value_or_default() == fgOutput->IsLowResMV();

        bool cantActivate = false;
        if (restartNeeded)
        {
            ImGui::TextColored(StatusWarn(), "请重启游戏以应用正确的 XeFG 设置！");
        }
        else
        {
            if (!correctMVs)
                ImGui::TextColored(StatusError(), "需要禁用膨胀运动矢量");

            if (!ignoreChecks && state.realExclusiveFullscreen)
            {
                cantActivate = true;
                ImGui::TextColored(StatusError(), "需要无边框显示模式！");
            }

            if (!ignoreChecks && outputIsHdr)
            {
                if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                    state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
                {
                    cantActivate = true;
                    ImGui::TextColored(StatusError(), "XeFG 仅支持 HDR10");
                }
            }
        }

        if (!correctMVs || cantActivate || ignoreChecks)
        {
            if (ImGui::Checkbox("忽略初始化检查", &ignoreChecks))
                config->FGXeFGIgnoreInitChecks = ignoreChecks;

            ShowTooltip("忽略 XeFG 的所有预检查\n"
                        "不要使用此选项跳过 UE 游戏的运动矢量大小警告！\n"
                        "它可能导致崩溃和画质变差！");
        }

        const int maxCount = fgOutput->GetMaxInterpolationCount();
        const bool dynamicMFG = maxCount >= 1 && config->FGXeFGDynamicMFG.value_or_default();

        // Frame Gen at 2X: Dynamic MFG only switches between off and 2X
        const bool dynamicOnOff =
            std::clamp(config->FGXeFGInterpolationCount.value_or_default(), 1, std::max(maxCount, 1)) == 1;

        ImGui::Text("当前 XeFG 状态：");
        ImGui::SameLine();
        if (!fgOutput->IsActive())
        {
            ImGui::TextColored(StatusError(), "关");
        }
        else
        {
            // Dynamic MFG's 1X keeps FG running with no generated frames
            const auto count = fgOutput->GetInterpolatedFrameCount();
            const auto color = count > 0 ? ImVec4(0.f, 1.f, 0.25f, 1.f) : ImVec4(1.0f, 0.647f, 0.0f, 1.f);
            ImGui::TextColored(toneMapColor(color), std::format("开 {}x", count + 1).c_str());

            if (dynamicMFG)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("（动态）");
            }
        }

        ImGui::BeginDisabled(!correctMVs || cantActivate);

        // Dynamic MFG: the Frame Gen setting is the most, the multiplier follows the game's frame rate
        if (dynamicMFG)
            RenderFGModeSlider(ctx, "最大帧生成###xefgMode", maxCount, &config->FGXeFGInterpolationCount,
                               "动态 MFG 可使用的最高倍率");
        else
            RenderFGModeSlider(ctx, "帧生成###xefgMode", maxCount, &config->FGXeFGInterpolationCount);

        if (maxCount >= 1)
        {
            ImGui::SameLine(0.0f, 16.0f);

            if (bool dynamic = dynamicMFG; ImGui::Checkbox("动态 MFG##xefg", &dynamic))
                config->FGXeFGDynamicMFG = dynamic;

            ShowTooltip("动态调整倍率以命中目标 FPS，\n"
                        "上限为帧生成设置（2X 时：帧生成开或关）。");
        }

        if (dynamicMFG)
        {
            // Applied on release, dragging doesn't restart Dynamic MFG every frame.
            // The slider's lowest step (9) is Auto, the lowest target is 10.
            constexpr float autoStep = 9.0f;
            static float xefgFpsTarget = std::max(config->FGXeFGFramerateTargetDMFG.value_or_default(), autoStep);
            const float fpsSliderMax = (float) std::max(200, refreshRate);

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            ImGui::SliderFloat("目标 FPS##xefg", &xefgFpsTarget, autoStep, fpsSliderMax,
                               xefgFpsTarget < autoStep + 0.5f ? "自动" : "%.0f", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                if (xefgFpsTarget < autoStep + 0.5f)
                    config->FGXeFGFramerateTargetDMFG.reset();
                else
                    config->FGXeFGFramerateTargetDMFG = xefgFpsTarget;
            }

            ShowTooltip("目标呈现 FPS，不会超过 FPS 限制。\n"
                        "自动：使用 FPS 限制，或略低于刷新率。");

            // The cap picks from 2X up, it needs a higher Frame Gen setting
            const bool targetAsCap = config->FGXeFGTargetAsCapDMFG.value_or_default() && !dynamicOnOff;

            if (!dynamicOnOff)
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool cap = targetAsCap; ImGui::Checkbox("用作 FPS 限制##xefg", &cap))
                    config->FGXeFGTargetAsCapDMFG = cap;

                ShowTooltip("同时将 FPS 限制到目标值，并使用最低\n"
                            "能达到该值的倍率。");
            }

            // The cap holds the target with its own count logic
            ImGui::BeginDisabled(targetAsCap);

            const char* roundings[] = { "最多目标", "围绕目标", "至少目标" };
            int rounding = std::clamp(config->FGXeFGRoundingDMFG.value_or_default(), 0, 2);

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo("##xefgRounding", &rounding, roundings, IM_ARRAYSIZE(roundings)))
                config->FGXeFGRoundingDMFG = rounding;

            ShowTooltip("FPS 可能落点的范围，因为倍率只能按步进：\n"
                        "最多、围绕或至少达到目标 FPS。");

            // At 2X, 1X is the only other step
            if (!dynamicOnOff)
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool allow1X = config->FGXeFGDynamicAllow1X.value_or_default();
                    ImGui::Checkbox("允许 1X##xefg", &allow1X))
                {
                    config->FGXeFGDynamicAllow1X = allow1X;
                }

                ShowTooltip("当游戏\n"
                            "单独就足以接近目标。");
            }

            ImGui::EndDisabled();
        }

        bool fgDV = config->FGXeFGDebugView.value_or_default();
        if (ImGui::Checkbox("调试视图##2", &fgDV))
        {
            config->FGXeFGDebugView = fgDV;

            if (config->FGXeFGDebugView.value_or_default())
            {
                state.fgChanged = true;
                LOG_DEBUG("调试视图已设置 FGChanged");
            }
        }
        ShowTooltip("启用 XeFG 调试视图");

        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 16.0f);
        bool fgBorderless = config->FGXeFGForceBorderless.value_or_default();
        if (ImGui::Checkbox("强制无边框", &fgBorderless))
            config->FGXeFGForceBorderless = fgBorderless;

        ShowTooltip("强制无边框显示模式\n\n"
                    "为获得最佳效果，请设置为全屏\n"
                    "分辨率与您的显示分辨率一致\n"
                    "可能导致一些不稳定问题。\n\n"
                    "需要重启游戏才能生效！");

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::BeginDisabled(!fgOutput->IsUsingHudlessAny() || XeFGProxy::SetUiCompositionState() == nullptr);
        bool fgCompositeUI = config->FGXeFGUIComposition.value_or_default();
        if (ImGui::Checkbox("UI 合成", &fgCompositeUI))
            config->FGXeFGUIComposition = fgCompositeUI;

        ShowTooltip("禁用 HUD/UI 插值\n"
                    "恢复为之前的 XeFG 2 行为\n\n"
                    "修复透明 HUD/UI 的伪影问题");
        ImGui::EndDisabled();

        // Disable this for now
        // ImGui::SameLine(0.0f, 16.0f);
        // ImGui::Checkbox("Only Generated##2", &state.fgOnlyGenerated);
        // ShowTooltip("Display only XeFG generated frames");

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("扩展 XeFG 设置"); ch.IsHeaderOpen())
        {
            ImGui::Spacing();
            if (ImGui::TreeNode("区域设置"))
            {
                ImGui::PushItemWidth(95.0f * menuResScale);
                int rectLeft = config->FGRectLeft.value_or(0);
                if (ImGui::InputInt("区域左##2", &rectLeft))
                    config->FGRectLeft = rectLeft;

                ImGui::SameLine(0.0f, 16.0f);
                int rectTop = config->FGRectTop.value_or(0);
                if (ImGui::InputInt("区域上##2", &rectTop))
                    config->FGRectTop = rectTop;

                int rectWidth = config->FGRectWidth.value_or(0);
                if (ImGui::InputInt("区域宽##2", &rectWidth))
                    config->FGRectWidth = rectWidth;

                ImGui::SameLine(0.0f, 16.0f);
                int rectHeight = config->FGRectHeight.value_or(0);
                if (ImGui::InputInt("区域高##2", &rectHeight))
                    config->FGRectHeight = rectHeight;

                ImGui::PopItemWidth();
                ShowTooltip("帧生成区域，为黑边内容调整##2");

                ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                     !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                if (ImGui::Button("重置帧生成区域##2"))
                {
                    config->FGRectLeft.reset();
                    config->FGRectTop.reset();
                    config->FGRectWidth.reset();
                    config->FGRectHeight.reset();
                }

                ShowTooltip("重置帧生成区域##2");

                ImGui::EndDisabled();
                ImGui::TreePop();
            }

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    // DLSSG controls
    if (auto dlssg = state.CurrentFG(); ScopedCard::Shows("fg_dlssg") && state.activeFgOutput == FGOutput::DLSSG &&
                                        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists() &&
                                        StreamlineProxy::LoadStreamline() && dlssg)
    {
        ScopedCard card { "DLSSG 设置" };

        if (state.activeFgNvngx == FGNvngxReplacement::None && (state.hdrOutputActive && outputIsHdr))
        {
            if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
            {
                ImGui::TextColored(StatusError(), "DLSSG 仅支持 HDR10");
            }
        }

        ImGui::Text("当前 DLSSG 状态：");
        ImGui::SameLine();
        if (auto count = state.dlssgDetectedInterpolationCount; count > 0)
        {
            ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), std::format("开 {}x", count + 1).c_str());
        }
        else
        {
            ImGui::TextColored(StatusError(), "关");
        }

        RenderFGModeSlider(ctx, "帧生成##dlssg", dlssg->GetMaxInterpolationCount(),
                           &config->FGDLSSGInterpolationCount);

        if (dlssg->GetMaxInterpolationCount() > 1)
        {
            if (dlssg->GetDMFGSupport())
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool dynamicMFG = config->FGDLSSGForceDMFG.value_or_default();
                    ImGui::Checkbox("强制动态 MFG", &dynamicMFG))
                {
                    config->FGDLSSGForceDMFG = dynamicMFG;
                    state.fgChanged = true;
                }

                ImGui::BeginDisabled(!config->FGDLSSGForceDMFG.value_or_default());
                static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
                float fpsSliderMax = (float) std::max(200, refreshRate);
                ImGui::SliderFloat("DMFG FPS 目标", &fpsTarget, 0, fpsSliderMax, "%.0f");

                ShowTooltip("限制值为 0 表示自动检测显示器刷新率");

                if (ImGui::Button("应用目标"))
                {
                    config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                }

                ImGui::SameLine(0.0f, 16.0f);

                if (ImGui::Button("重置目标"))
                {
                    fpsTarget = 0.0f;
                    config->FGDLSSGFramerateTargetDMFG.reset();
                }

                ImGui::EndDisabled();
            }
        }

        bool useGamesMarkers = config->FGDLSSGUseGamesReflexMarkers.value_or_default();
        // Vulkan games' markers don't reach OptiScaler's Streamline
        ImGui::BeginDisabled(!ReflexHooks::gameIsSendingMarkers() || state.swapchainApi == API::Vulkan);
        if (ImGui::Checkbox("使用游戏自带的 Reflex 标记", &useGamesMarkers))
        {
            config->FGDLSSGUseGamesReflexMarkers = useGamesMarkers;
            LOG_DEBUG("已设置 FGDLSSGUseGamesReflexMarkers：{}", useGamesMarkers);
        }
        ImGui::EndDisabled();
    }

    // Flip meter controls, the generated frames come from the Nvngx replacement
    if (ScopedCard::Shows("fg_flipmeter") && state.activeFgOutput == FGOutput::FlipMeter &&
        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists() && state.CurrentFG() != nullptr)
    {
        ScopedCard card { "翻转节流设置" };

        auto flipMeter = dynamic_cast<FlipMeterFG_Dx12*>(state.CurrentFG());
        auto swapChain = flipMeter != nullptr ? flipMeter->SwapChain() : nullptr;

        RenderFGModeSlider(ctx, "帧生成##flipmeter", flipMeter != nullptr ? flipMeter->MaxInterpolationCount() : 1,
                           &config->FGFlipMeterInterpolationCount);

        if (swapChain != nullptr)
        {
            const auto status = swapChain->GetStatus();

            if (status.metering)
            {
                ImGui::TextColored(StatusOk(), "驱动节流：%s，%uX",
                                   status.mode == AmdExtFlipMeterMode_FrameIndex ? "帧索引模式" : "自适应模式",
                                   status.generatedFrames + 1);
                ShowTooltip("AMD 驱动翻转节流会保持生成帧的翻转\n"
                            "自适应模式由 FSR 提供，每次节流一帧生成帧，\n"
                            "帧索引模式节流更多");
            }
            else if (status.generatedFrames == 0)
            {
                ImGui::TextDisabled("驱动节流：未生成帧时关闭");
                ShowTooltip("帧生成已关闭，或游戏未发送\n"
                            "它的输入，例如在菜单和加载画面中");
            }
            else
            {
                ImGui::TextColored(StatusError(), "驱动节流：关闭，仅显示游戏帧");
            }

            if (!status.coversOutput)
                ImGui::TextColored(StatusError(), "需要独占全屏或覆盖整个显示器的窗口");

            if (FAILED(status.configResult))
            {
                ImGui::TextColored(StatusError(), "驱动拒绝了配置：%08X，状态 %u",
                                   (UINT) status.configResult, status.configStatus);
            }

            const int providerMax = FlipMeterFG_Dx12::ProviderMaxInterpolationCount();
            const int driverMax = (int) swapChain->MaxGeneratedFrames();

            if (driverMax < providerMax)
            {
                // The file's path or why it couldn't be written, kept while the menu is open
                static std::optional<std::expected<std::filesystem::path, std::string>> regFile;

                // At least 6X, like DLSSG, so other FG replacements don't need a new one. The driver allows 15.
                const UINT regFrames = (UINT) std::clamp(std::max(providerMax, 5), 1, 15);

                ImGui::TextColored(StatusWarn(), "驱动最高允许 %dX，FG 替换为 %dX", driverMax + 1,
                                   providerMax + 1);
                ImGui::TextWrapped("AMD 驱动默认仅节流 2X，更高倍率需要注册表设置。\n"
                                   "下方按钮会在游戏 exe 旁创建 .reg 文件。\n"
                                   "运行它并重启电脑。");

                if (ImGui::Button("创建 .reg 文件##flipmeter"))
                    regFile = swapChain->WriteMaxGenFramesRegFile(regFrames);

                ShowTooltip(std::format("将 GPU 的 DWORD Gmx_DriverFramePacingMaxGenFrames 设置为 {}（{}X）\n"
                                        "驱动注册表项位于 HKLM\\SYSTEM\\CurrentControlSet\\Control\\Class\\\n"
                                        "{{4d36e968-e325-11ce-bfc1-08002be10318}}",
                                        regFrames, regFrames + 1)
                                .c_str());

                if (regFile.has_value() && regFile->has_value())
                {
                    ImGui::TextColored(StatusOk(), "已保存 %s", wstring_to_string(regFile->value().wstring()).c_str());
                    ImGui::TextWrapped("双击它并允许更改（需要管理员权限），然后重启 Windows。 "
                                       "文件中说明了如何撤销。");
                }
                else if (regFile.has_value())
                {
                    ImGui::TextColored(StatusError(), "创建失败：%s", regFile->error().c_str());
                }
            }
        }
    }

    if (auto reprojectionOutput = state.CurrentFG();
        ScopedCard::Shows("fg_reprojection") && reprojectionOutput && reprojectionOutput->HasReprojection())
    {
        ScopedCard card { "重投影设置" };

        if (reprojectionOutput->IsActive() && reprojectionOutput->IsReprojectionActive())
        {
            ImGui::Text("相机旋转更新量：%.1fms",
                        (float) reprojectionOutput->GetLastTimeSinceSimStartNs() / 1'000'000.f);
        }
        else if (reprojectionOutput->IsActive() && reprojectionOutput->GetReproject() &&
                 reprojectionOutput->GetReproject()->IsBypassed())
        {
            ImGui::TextColored(StatusWarn(), "已暂停，检测到第三人称");
        }
        else
        {
            ImGui::TextDisabled("未更新相机旋转");
        }

        if (state.activeFgOutput == FGOutput::Reprojection)
        {
            RenderFGModeSlider(ctx, "重投影##mode", 0, nullptr);
        }
        else
        {
            bool reprojectionActive = config->FGReprojectionEnabled.value_or_default();
            if (ImGui::Checkbox("启用##5", &reprojectionActive))
            {
                config->FGReprojectionEnabled = reprojectionActive;
                LOG_DEBUG("Reprojection enabled: {}", reprojectionActive);
            }
            ShowTooltip("启用重投影");
        }

        ImGui::SameLine();

        ImGui::Checkbox("显示静态元素", &state.fgHudlessCompare);
        ShowTooltip("用于微调深度截断\n"
                    "显示 UI 与深度截断区域\n"
                    "调整深度截断，使只有枪和手等物体被标记");

        ImGui::Spacing();

        // clang-format off
        static std::vector<MenuOption<ReprojectionFill>> fillModes = {
            { ReprojectionFill::SoftEdge, "柔和边缘" },
            { ReprojectionFill::StrechEdge, "拉伸边缘" },
            { ReprojectionFill::Dithering, "抖动" },
            { ReprojectionFill::Noise, "噪点" },
            { ReprojectionFill::Debug, "调试" }
        };
        // clang-format on

        // need to have a value before combo
        if (!config->ReprojectionFillMode.has_value())
            config->ReprojectionFillMode = config->ReprojectionFillMode.value_or_default();

        PopulateCombo("边缘填充模式", config->ReprojectionFillMode, fillModes);
        ShowTooltip("柔和边缘与拉伸会用边缘像素填充屏幕边缘，\n"
                    "柔和边缘越向外越模糊\n"
                    "抖动与噪点使用未重投影的图像作为填充\n"
                    "然后在边缘进行一些混合以欺骗视觉");

        float cutoff = config->ReprojectionDepthCutoff.value_or_default();
        if (ImGui::SliderFloat("深度截断", &cutoff, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
            config->ReprojectionDepthCutoff = cutoff;
        ShowTooltip("选择相机附近应显示在重投影画面上的元素数量。\n"
                    "应显示在重投影图像之上。\n"
                    "这可以防止你的枪被奇怪地移动。\n\n"
                    "可使用“显示静态元素”来帮助调整\n"
                    "虚幻引擎游戏通常约为 0.10\n"
                    "《赛博朋克》约为 0.02");

        uint32_t cutoffExpandPx = config->ReprojectionCutoffExpand.value_or_default();
        if (SliderUInt("截断扩展", &cutoffExpandPx, 0, 8))
            config->ReprojectionCutoffExpand = cutoffExpandPx;
        ShowTooltip("按此深度像素数扩展截断区域\n"
                    "仅在看到截断过程留下轮廓时使用");

        if (bool lateLatch = config->ReprojectionLateLatch.value_or_default();
            ImGui::Checkbox("延迟锁存鼠标", &lateLatch))
        {
            config->ReprojectionLateLatch = lateLatch;
        }
        ShowTooltip("GPU 开始重投影时读取鼠标\n"
                    "而不是在 CPU 呈现帧时读取。\n"
                    "降低延迟，尤其是在 GPU 受限时");

        if (bool directionCache = config->ReprojectionDirectionCache.value_or_default();
            ImGui::Checkbox("方向缓存", &directionCache))
        {
            config->ReprojectionDirectionCache = directionCache;
        }
        ShowTooltip("记住前几帧相机周围的内容\n"
                    "并用它填充屏幕边缘和手下方的区域。\n"
                    "边缘填充模式仅用于缓存中没有内容的区域。\n"
                    "调试模式下以蓝色调显示");

        const auto reproject = reprojectionOutput->GetReproject();
        const bool hasMotionStats = reproject && reproject->HasMotionStats();

        bool parallaxAuto = config->ReprojectionParallaxAuto.value_or_default();
        const float parallaxRowStart = ImGui::GetCursorPosX();

        if (ImGui::Checkbox("自动##parallax", &parallaxAuto))
            config->ReprojectionParallaxAuto = parallaxAuto;
        ShowTooltip("根据游戏运动矢量估算视差缩放\n"
                    "需要走动一段时间，在此之前缓存仅跟随旋转");

        ImGui::SameLine();

        // The checkbox takes part of the row, the slider gets what is left so the label still fits
        const float parallaxSliderWidth = ImGui::CalcItemWidth() - (ImGui::GetCursorPosX() - parallaxRowStart);

        if (parallaxAuto)
        {
            float estimate = reproject ? reproject->ParallaxScaleEstimate() : 0.0f;

            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(parallaxSliderWidth);
            ImGui::SliderFloat("缓存视差缩放", &estimate, 0.0f, 10000.0f,
                               estimate > 0.0f ? "%.1f"
                                               : (hasMotionStats ? "估算中……" : "无可用运动矢量"),
                               ImGuiSliderFlags_Logarithmic);
            ImGui::EndDisabled();
        }
        else
        {
            float parallaxScale = config->ReprojectionParallaxScale.value_or_default();
            ImGui::SetNextItemWidth(parallaxSliderWidth);
            if (ImGui::SliderFloat("缓存视差缩放", &parallaxScale, 0.0f, 10000.0f, "%.1f",
                                   ImGuiSliderFlags_Logarithmic))
            {
                config->ReprojectionParallaxScale = parallaxScale;
            }
        }
        ShowTooltip("将深度距离转换为相机位置的单位\n"
                    "以便行走时缓存能跟随相机。\n"
                    "0 = 仅跟随旋转\n"
                    "错误的值会导致行走时缓存填充抖动\n"
                    "也用于第三人称检测\n"
                    "虚幻引擎通常约为 1000");

        if (bool thirdPersonDetection = config->ReprojectionThirdPersonDetection.value_or_default();
            ImGui::Checkbox("第三人称时暂停", &thirdPersonDetection))
        {
            config->ReprojectionThirdPersonDetection = thirdPersonDetection;
        }
        ShowTooltip("重投影会让角色随相机摆动，\n"
                    "因此当屏幕中央有物体跟随相机时会暂停。\n"
                    "使用游戏的运动矢量与缓存视差缩放。\n"
                    "载具驾驶舱也可能被视为第三人称。\n"
                    "禁用后重投影将始终保持激活");

        ImGui::SameLine();

        if (!hasMotionStats)
        {
            ImGui::TextDisabled("（无可用运动矢量）");
        }
        else if (reproject->ParallaxScale() <= 0.0f)
        {
            ImGui::TextDisabled("（需要视差缩放）");
        }
        else if (reproject->ThirdPersonDetected())
        {
            ImGui::TextColored(StatusWarn(), "检测到第三人称（%.2f，环绕 %.2f）", reproject->ThirdPersonScore(),
                               reproject->OrbitScore());
        }
        else
        {
            ImGui::TextDisabled("第一人称（%.2f，环绕 %.2f）", reproject->ThirdPersonScore(),
                                reproject->OrbitScore());
        }
    }

    // OptiFG
    if (ScopedCard::Shows("fg_optifg") && state.api != API::Vulkan && state.currentFGSwapchain != nullptr &&
        state.activeFgInput == FGInput::Upscaler)
    {
        ScopedCard card { "OptiFG 设置" };

        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            ((state.activeFgOutput == FGOutput::FSRFG && FfxApiProxy::IsFsrFgReady()) ||
             (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() != nullptr) ||
             (state.activeFgOutput == FGOutput::DLSSG && StreamlineProxy::Module() != nullptr) ||
             state.activeFgOutput == FGOutput::Reprojection || state.activeFgOutput == FGOutput::FlipMeter))
        {
            const bool dx11HudfixTracking = state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12;
            const bool hudfixTrackingSupported =
                !Config::Instance()->FGDisableHUDFix.value_or_default() &&
                (state.swapchainInteropApi == SwapchainInteropApi::None || dx11HudfixTracking);

            if (hudfixTrackingSupported)
            {
                bool fgHudfix = config->FGHUDFix.value_or_default();

                if (ImGui::Checkbox("HUDFix", &fgHudfix))
                {
                    config->FGHUDFix = fgHudfix;
                    LOG_DEBUG("已设置 FGHUDFix：{}", fgHudfix);
                    state.clearCapturedHudlesses = true;
                    state.fgChanged = true;
                }

                ShowTooltip("启用 HUD 稳定性修复，可能导致崩溃！");

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                ImGui::SameLine(0.0f, 16.0f);
                ImGui::PushItemWidth(95.0f * menuResScale);
                int hudFixLimit = config->FGHUDLimit.value_or_default();
                if (ImGui::InputInt("限制", &hudFixLimit))
                {
                    if (hudFixLimit < 1)
                        hudFixLimit = 1;
                    else if (hudFixLimit > 999)
                        hudFixLimit = 999;

                    config->FGHUDLimit = hudFixLimit;
                    LOG_DEBUG("已设置 FGHUDLimit：{}", hudFixLimit);
                }
                ShowTooltip("延迟 HUDless 捕获，过高的值可能导致崩溃！");

                ImGui::SameLine(0.0f, 16.0f);
                if (ImGui::Button("重置##2"))
                    _showHudlessWindow = !_showHudlessWindow;

                ImGui::EndDisabled();

                auto hudExtended = config->FGHUDFixExtended.value_or_default();
                if (ImGui::Checkbox("扩展", &hudExtended))
                {
                    LOG_DEBUG("已设置 FGHUDFixExtended：{}", hudExtended);
                    config->FGHUDFixExtended = hudExtended;
                }
                ShowTooltip("对可能的 HUDless 进行扩展格式检查\n可能导致崩溃和卡顿！");
                ImGui::SameLine(0.0f, 16.0f);

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                auto immediate = config->FGImmediateCapture.value_or_default();
                if (ImGui::Checkbox("立即捕获", &immediate))
                {
                    LOG_DEBUG("已设置 FGImmediateCapture：{}", immediate);
                    config->FGImmediateCapture = immediate;
                }
                ShowTooltip("在着色器执行前捕获资源。\n提高 HUDless"
                            "捕获几率，但可能捕获不必要的资源。");

                ImGui::PopItemWidth();

                ImGui::EndDisabled();
            }

            bool depthScale = config->FGEnableDepthScale.value_or_default();
            if (ImGui::Checkbox("缩放深度以修复 DLSS RR", &depthScale))
                config->FGEnableDepthScale = depthScale;
            ShowTooltip("修复 DLSS-D 错误的深度输入");

            bool resourceFlip = config->FGResourceFlip.value_or_default();
            if (ImGui::Checkbox("翻转（Unity）", &resourceFlip))
                config->FGResourceFlip = resourceFlip;
            ShowTooltip("翻转 Unity 游戏的速度与深度资源");

            ImGui::SameLine(0.0f, 16.0f);

            bool hudlessFlip = config->FGHudlessFlip.value_or_default();
            if (ImGui::Checkbox("翻转 HUDless", &hudlessFlip))
                config->FGHudlessFlip = hudlessFlip;
            ShowTooltip("翻转 HUDless 纹理\n游戏也可通过 Opti.Hudless.Flip 参数请求");

            ImGui::SameLine(0.0f, 16.0f);

            bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
            if (ImGui::Checkbox("翻转使用偏移", &resourceFlipOffset))
                config->FGResourceFlipOffset = resourceFlipOffset;
            ShowTooltip("使用高度差作为偏移");

            if (auto ch = ScopedCollapsingHeader("高级 OptiFG 设置"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};

                ImGui::Spacing();

                auto disablehudfix = config->FGDisableHUDFix.value_or_default();
                {
                    if (ImGui::Checkbox("禁用 HUDFix 资源跟踪", &disablehudfix))
                    {
                        config->FGDisableHUDFix = disablehudfix;
                        LOG_DEBUG("Enabled FGDisableHUDFix: {}", disablehudfix);
                    }
                    ShowTooltip("禁用 HUDFix 资源跟踪并\n"
                                "降低 CPU 开销。\n"
                                "适用于能很好处理 HUD 的 MLFG。\n\n"
                                "更改后，请保存设置并重启。");
                }

                if (hudfixTrackingSupported)
                {
                    ImGui::Spacing();

                    auto rb = config->FGResourceBlocking.value_or_default();
                    if (ImGui::Checkbox("资源阻止", &rb))
                    {
                        config->FGResourceBlocking = rb;
                        LOG_DEBUG("已设置 FGResourceBlocking：{}", rb);
                    }
                    ShowTooltip("阻止不常用资源被用作 HUDless\n"
                                "以防止闪烁和其他问题\n\n"
                                "启用/禁用 HUDfix 将重置阻止列表！");

                    ImGui::SameLine(0.0f, 16.0f);

                    auto rrc = config->FGRelaxedResolutionCheck.value_or_default();
                    if (ImGui::Checkbox("放宽资源检查", &rrc))
                    {
                        config->FGRelaxedResolutionCheck = rrc;
                        LOG_DEBUG("已设置 FGRelaxedResolutionCheck：{}", rrc);
                    }
                    ShowTooltip("将 HUDless 的分辨率检查放宽 32 像素\n"
                                "帮助某些使用黑边的游戏\n"
                                "以及特殊分辨率和屏幕比例（例如《巫师3》）");

                    ImGui::BeginDisabled(state.fgResetCapturedResources);
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    if (ImGui::Checkbox("帧生成创建列表", &state.fgCaptureResources))
                    {
                        if (!state.fgCaptureResources)
                            config->FGHUDLimit = 1;
                        else
                            state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::SameLine(0.0f, 16.0f);
                    if (ImGui::Checkbox("帧生成使用列表", &state.fgOnlyUseCapturedResources))
                    {
                        if (state.fgCaptureResources)
                        {
                            state.fgCaptureResources = false;
                            config->FGHUDLimit = 1;
                        }
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::Text("(%d)", state.fgCapturedResourceCount);

                    ImGui::PopItemWidth();

                    ImGui::SameLine(0.0f, 16.0f);

                    if (ImGui::Button("重置列表"))
                    {
                        LOG_DEBUG("正在重置已捕获的资源列表");

                        state.fgResetCapturedResources = true;
                        state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::Spacing();
                    if (ImGui::TreeNode("跟踪设置"))
                    {
                        ImGui::BeginDisabled(dx11HudfixTracking);

                        auto ath = config->FGAlwaysTrackHeaps.value_or_default();
                        if (ImGui::Checkbox("始终跟踪堆", &ath))
                        {
                            config->FGAlwaysTrackHeaps = ath;
                            LOG_DEBUG("已设置 FGAlwaysTrackHeaps：{}", ath);
                        }
                        ImGui::EndDisabled();

                        ShowTooltip(dx11HudfixTracking
                                        ? "仅限 D3D12；不适用于 DX11。"
                                        : "始终跟踪资源，可能导致性能问题\n，但也可能"
                                          "修复与 HUDfix 相关的崩溃！");

                        auto disableRTV = config->FGHudfixDisableRTV.value_or_default();
                        if (ImGui::Checkbox("禁用 RTV 跟踪", &disableRTV))
                            config->FGHudfixDisableRTV = disableRTV;
                        ShowTooltip("禁用 CreateRenderTargetView 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSRV = config->FGHudfixDisableSRV.value_or_default();
                        if (ImGui::Checkbox("禁用 SRV 跟踪", &disableSRV))
                            config->FGHudfixDisableSRV = disableSRV;
                        ShowTooltip("禁用 CreateShaderResourceView 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        auto disableUAV = config->FGHudfixDisableUAV.value_or_default();
                        if (ImGui::Checkbox("禁用 UAV 跟踪", &disableUAV))
                            config->FGHudfixDisableUAV = disableUAV;
                        ShowTooltip("禁用 CreateUnorderedAccessView 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableOM = config->FGHudfixDisableOM.value_or_default();
                        if (ImGui::Checkbox("禁用 OM 跟踪", &disableOM))
                            config->FGHudfixDisableOM = disableOM;
                        ShowTooltip("禁用 OMSetRenderTargets 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        auto disableSCR = config->FGHudfixDisableSCR.value_or_default();
                        if (ImGui::Checkbox("禁用 SCR 跟踪", &disableSCR))
                            config->FGHudfixDisableSCR = disableSCR;
                        ShowTooltip("禁用 SetComputeRootDescriptorTable 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSGR = config->FGHudfixDisableSGR.value_or_default();
                        if (ImGui::Checkbox("禁用 SGR 跟踪", &disableSGR))
                            config->FGHudfixDisableSGR = disableSGR;
                        ShowTooltip("禁用 SetGraphicsRootDescriptorTable 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::Spacing();

                        auto disableDI = config->FGHudfixDisableDI.value_or_default();
                        if (ImGui::Checkbox("禁用 DI 跟踪", &disableDI))
                            config->FGHudfixDisableDI = disableDI;
                        ShowTooltip("禁用 DrawInstanced 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableDII = config->FGHudfixDisableDII.value_or_default();
                        if (ImGui::Checkbox("禁用 DII 跟踪", &disableDII))
                            config->FGHudfixDisableDII = disableDII;
                        ShowTooltip("禁用 DrawIndexedInstanced 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        auto disableDispatch = config->FGHudfixDisableDispatch.value_or_default();
                        if (ImGui::Checkbox("禁用 Dispatch 跟踪", &disableDispatch))
                            config->FGHudfixDisableDispatch = disableDispatch;
                        ShowTooltip("禁用 Dispatch 的跟踪\n"
                                    "这可能有助于过滤错误的 HUDless 资源");

                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                if (ImGui::TreeNode("资源设置"))
                {
                    bool makeMVCopies = config->FGMakeMVCopy.value_or_default();
                    if (ImGui::Checkbox("帧生成复制运动矢量", &makeMVCopies))
                        config->FGMakeMVCopy = makeMVCopies;
                    ShowTooltip("复制运动矢量以供 OptiFG 使用\n"
                                "以防止可能发生的损坏");

                    bool makeDepthCopies = config->FGMakeDepthCopy.value_or_default();
                    if (ImGui::Checkbox("帧生成复制深度", &makeDepthCopies))
                        config->FGMakeDepthCopy = makeDepthCopies;
                    ShowTooltip("复制深度以供 OptiFG 使用\n"
                                "以防止可能发生的损坏");

                    ImGui::PushItemWidth(115.0f * menuResScale);
                    float depthScaleMax = config->FGDepthScaleMax.value_or_default();
                    if (ImGui::InputFloat("帧生成深度缩放上限", &depthScaleMax, 10.0f, 100.0f, "%.1f"))
                        config->FGDepthScaleMax = depthScaleMax;
                    ShowTooltip("深度值将除以该值");
                    ImGui::PopItemWidth();

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                if (ImGui::TreeNode("同步设置"))
                {
                    bool useMutexForPresent = config->FGUseMutexForSwapchain.value_or_default();
                    if (ImGui::Checkbox("呈现时使用互斥锁", &useMutexForPresent))
                        config->FGUseMutexForSwapchain = useMutexForPresent;
                    ShowTooltip("使用互斥锁防止帧生成失同步和崩溃\n"
                                "禁用它可能提高性能但降低稳定性");

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
        else if (currentFeature == nullptr || currentFeature->IsFrozen())
        {
            ImGui::Text("超分器未激活"); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFsrFgReady())
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               "缺少 amd_fidelityfx_dx12.dll！"); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() == nullptr)
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               "缺少 libxess_fg.dll！"); // Probably never will be visible
        }
    }

    if (ScopedCard::Shows("fg_optifg") && state.api == API::Vulkan && state.currentVkFG != nullptr &&
        state.activeFgInput == FGInput::Upscaler)
    {
        ScopedCard card { "OptiFG 设置" };

        if (!config->FGDisableHUDFix.value_or_default())
        {
            bool fgHudfix = config->FGHUDFix.value_or_default();
            if (ImGui::Checkbox("HUDFix", &fgHudfix))
                config->FGHUDFix = fgHudfix;
            ShowTooltip("启用 HUD 稳定性修复，可能导致崩溃！");

            ImGui::BeginDisabled(!fgHudfix);

            ImGui::SameLine(0.0f, 16.0f);
            ImGui::PushItemWidth(95.0f * menuResScale);
            int hudFixLimit = config->FGHUDLimit.value_or_default();
            if (ImGui::InputInt("限制", &hudFixLimit))
                config->FGHUDLimit = std::clamp(hudFixLimit, 1, 999);
            ShowTooltip("延迟 HUDless 捕获，过高的值可能导致崩溃！");
            ImGui::PopItemWidth();

            ImGui::SameLine(0.0f, 16.0f);
            auto hudExtended = config->FGHUDFixExtended.value_or_default();
            if (ImGui::Checkbox("扩展", &hudExtended))
                config->FGHUDFixExtended = hudExtended;
            ShowTooltip("接受任意格式的 HUDless 候选");

            auto uiTexture = config->FGHUDFixUITexture.value_or_default();
            if (ImGui::Checkbox("UI 纹理", &uiTexture))
                config->FGHUDFixUITexture = uiTexture;
            ShowTooltip("使用游戏在最后计算通道中混合到帧上的 UI 纹理");

            const bool dlssgOutput = state.activeFgOutput == FGOutput::DLSSG;
            ImGui::BeginDisabled(!uiTexture || dlssgOutput);
            ImGui::SameLine(0.0f, 16.0f);

            auto uiUnblend = !dlssgOutput && config->FGHUDFixUIUnblend.value_or(true);
            if (ImGui::Checkbox("取消混合 UI", &uiUnblend))
                config->FGHUDFixUIUnblend = uiUnblend;
            ShowTooltip("从帧中移除 UI 纹理以重建 HUDless\n"
                        "FSR-FG 只能通过这种方式获取 UI，DLSSG 则直接使用 UI 纹理");

            ImGui::EndDisabled();

            ImGui::EndDisabled();
        }

        bool resourceFlip = config->FGResourceFlip.value_or_default();
        if (ImGui::Checkbox("翻转（Unity）", &resourceFlip))
            config->FGResourceFlip = resourceFlip;
        ShowTooltip("翻转 Unity 游戏的速度与深度资源");

        ImGui::SameLine(0.0f, 16.0f);

        bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
        if (ImGui::Checkbox("翻转使用偏移", &resourceFlipOffset))
            config->FGResourceFlipOffset = resourceFlipOffset;
        ShowTooltip("使用高度差作为偏移");
    }

    const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;
    if (ScopedCard::Shows("fg_nvngx") && activeNvngxFg != FGNvngxReplacement::None)
    {
        ScopedCard card;

        if (activeNvngxFg == FGNvngxReplacement::Nukems)
        {
            SeparatorWithHelpMarker("帧生成（通过 Nukem 的 DLSSG 使用 FSR3-FG）",
                                    "需要 Nukem 的 dlssg_to_fsr3 dll");

            if (!state.nukemsFgFileAvailable)
            {
                ImGui::TextColored(StatusError(), "请将 dlssg_to_fsr3_amd_is_better.dll 放入 OptiScaler 文件夹");
            }
        }
        else if (activeNvngxFg == FGNvngxReplacement::Arturs)
        {
            SeparatorWithHelpMarker("帧生成（通过 DLSS Enabler 的 FSR3-MFG）",
                                    "DLSS Enabler 以 dlss-enabler-headless.dll 形式加载");

            if (!state.artursFgFileAvailable)
            {
                ImGui::TextColored(StatusError(), "请将 dlss-enabler-headless.dll 放入 OptiScaler 文件夹");
            }

            ImGui::TextColored(StatusWarn(), "使用 DLSS Enabler 的部分功能");
        }
        else if (activeNvngxFg == FGNvngxReplacement::FFX)
        {
            SeparatorWithHelpMarker("帧生成（通过 FFX 的 FSRFG）", "FFX 使用 DLSSG 交换链");
        }
        else if (activeNvngxFg == FGNvngxReplacement::Custom)
        {
            SeparatorWithHelpMarker(StrFmt("帧生成（%s）", state.activeFgNvngxCustom.c_str()).c_str(),
                                    "Custom DLSSG from the custom folder");
        }
        else if (activeNvngxFg == FGNvngxReplacement::Combo)
        {
            SeparatorWithHelpMarker("帧生成（Enabler + FFX）",
                                    "中间假帧使用 FFX，其余使用 Enabler\n\n2x - FFX\n"
                                    "3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler");
        }

        if (state.activeFgInput == FGInput::NvngxFG)
        {

            bool dmfgActive = state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default();

            if (!ReflexHooks::isReflexHooked())
            {
                ImGui::TextColored(StatusError(), "Reflex 未挂钩");
                ImGui::Text("如果您使用的是 AMD/Intel 显卡，请确保已安装 Fakenvapi");
            }
            else if (ReflexHooks::dlssgFrameCountToGenerate() == 0 && !dmfgActive)
            {
                ImGui::Text("请在游戏选项中启用 DLSS 帧生成\n"
                            "您可能需要先选择 DLSS");
            }

            if (state.swapchainApi == DX12)
            {
                ImGui::Text("当前 DLSSG 状态：");
                ImGui::SameLine();
                if (auto count = state.dlssgDetectedInterpolationCount; count > 0)
                {
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)),
                                       std::format("开 {}x", count + 1).c_str());
                }
                else
                {
                    ImGui::TextColored(StatusError(), "关");
                }

                // Issue mostly shows up on AMD on Windows on pre-RDNA3 in some non-UE games
                // Hide to reduce confusion, config is still read
                const bool isUnrealEngine = State::Instance().NVNGX_Engine == NVSDK_NGX_ENGINE_TYPE_UNREAL ||
                                            State::Instance().gameQuirks & GameQuirk::ForceUnrealEngine;
                const bool isDllProxyNvngxType = activeNvngxFg == FGNvngxReplacement::Nukems ||
                                                 activeNvngxFg == FGNvngxReplacement::Arturs ||
                                                 activeNvngxFg == FGNvngxReplacement::Custom;
                if (isDllProxyNvngxType && !primaryGpu.dlssCapable && primaryGpu.fsr4Support == FSR4Support::None &&
                    !primaryGpu.d3d12IsVkd3dProton && !isUnrealEngine)
                {
                    if (bool makeDepthCopy = config->NvngxFGMakeDepthCopy.value_or_default();
                        ImGui::Checkbox("修复损坏的画面", &makeDepthCopy))
                    {
                        config->NvngxFGMakeDepthCopy = makeDepthCopy;
                    }
                    ShowTooltip("复制深度缓冲区\n可修复 AMD GPU 在 Windows 下某些游戏中的画面损坏"
                                "GPU\n可能导致卡顿，因此最好仅在必要时使用");
                }
            }
        }

        bool isLoaded = false;
        if (state.swapchainApi == Vulkan)
            isLoaded = Nvngx_FG::isVulkanAvailable();
        if (state.swapchainApi == DX12)
            isLoaded = Nvngx_FG::isDx12Available();

        if (isLoaded)
        {
            // DLSS Enabler's own, or any custom provider reporting its antighosting version
            if (Nvngx_FG::supportsDispatchFlags())
            {
                auto antighostingVer = Nvngx_FG::extraVersion();

                if (activeNvngxFg == FGNvngxReplacement::Arturs || activeNvngxFg == FGNvngxReplacement::Combo)
                {
                    auto featureVer = Nvngx_FG::version();
                    ImGui::Text("DE 版本：%d.%d.%d.%d   GB 版本：%d.%d", featureVer.major, featureVer.minor,
                                featureVer.patch, featureVer.reserved, antighostingVer.major, antighostingVer.minor);
                }
                else
                {
                    ImGui::Text("GB 版本：%d.%d", antighostingVer.major, antighostingVer.minor);
                }

                static std::vector<FlagDefinition> common_flags = {
                    { "抗拖影（GB）", 0x00100000, "启用抗拖影校正" },
                    { "时域 HUD 固定", 0x04000000, "启用时域 HUD 固定（呈现-后备缓冲稳定性）" }
                };

                static std::vector<FlagDefinition> uncommon_flags = {
                    //{ "Hudless UI mask", 0x02000000, "Use HUD-less as UI mask (DL2 inverted semantics)" },
                    { "HUD 插值", 0x08000000, "HUD 光流插值（0=传统固定呈现，1=光流扭曲）" },
                    { "忽略 UI 纹理", 0x10000000, "忽略专用 DLSSG.UI 纹理（强制使用传统 HUD 路径）" },
                    //{ "Dp4a active", 0x20000000, "OF pipeline using dp4a-accelerated SSD (SM 6.4+)" },
                    { "固定后备缓冲", 0x40000000, "在 MFG 帧内将 DLSSG.Backbuffer 固定到子帧-1 快照" }
                };

                static std::vector<FlagDefinition> debug_flags = {
                    { "抗拖影红色着色", 0x00200000, "调试：在已校正像素上显示红色" },
                    { "抗拖影分屏对比", 0x00400000, "调试：分屏对比" },
                    { "帧索引线", 0x00010000, "" },
                    { "HUD 检测", 0x00020000, "" },
                    { "去遮挡着色", 0x00040000, "" },
                    { "伪影检测", 0x00080000, "" },
                    { "相机运动矢量调试", 0x00800000, "调试：在使用相机运动矢量回退处显示蓝色" },
                    { "通用可视化", 0x01000000, "调试：梯形区域可视化" }
                };

                uint32_t temp_flags = config->NvngxFGDispatchFlags.value_or_default();
                bool changed = false;

                ImGui::Text("原始 DispatchFlags：");
                changed |= ImGui::InputScalar("##RawFlags", ImGuiDataType_U32, &temp_flags, NULL, NULL, "%08X",
                                              ImGuiInputTextFlags_CharsHexadecimal);

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                if (bool showDebug = config->NvngxFGShowDebug.value_or_default();
                    ImGui::Checkbox("显示调试", &showDebug))
                {
                    config->NvngxFGShowDebug = showDebug;
                }
                ShowTooltip("调试标志需要此项才能正常工作");

                ImGui::Spacing();

                if (auto ch = ScopedCollapsingHeader("激活的 DispatchFlags"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};

                    auto render_flags = [&](const std::vector<FlagDefinition>& flags)
                    {
                        for (const auto& flag : flags)
                        {
                            changed |= ImGui::CheckboxFlags(flag.name.c_str(), &temp_flags, flag.mask);

                            if (ImGui::IsItemHovered() && !flag.description.empty())
                            {
                                ImGui::SetTooltip("%s", flag.description.c_str());
                            }
                        }
                    };

                    ImGui::TextDisabled("常见");
                    render_flags(common_flags);

                    ImGui::Spacing();
                    ImGui::TextDisabled("不常见");
                    render_flags(uncommon_flags);

                    if (config->NvngxFGShowDebug.value_or_default())
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled("调试");
                        render_flags(debug_flags);
                    }
                }

                if (changed)
                {
                    config->NvngxFGDispatchFlags = temp_flags;
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                if (ImGui::Checkbox("启用调试视图", &state.dlssgDebugView))
                {
                    Nvngx_FG::setDebugView(state.dlssgDebugView);
                }
                if (ImGui::Checkbox("仅插值帧", &state.dlssgInterpolatedOnly))
                {
                    Nvngx_FG::setInterpolatedOnly(state.dlssgInterpolatedOnly);
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::FFX || activeNvngxFg == FGNvngxReplacement::Combo)
            {
                if (_ffxFGIndex < 0 || _ffxFGIndex >= (int) state.ffxFGVersionNames.size())
                    _ffxFGIndex = config->FfxFGIndex.value_or_default();

                if (state.ffxFGVersionNames.size() > 0)
                {
                    ImGui::PushItemWidth(135.0f * menuResScale);

                    auto currentName = StrFmt("FSR %s", state.ffxFGVersionNames[_ffxFGIndex]);
                    if (ImGui::BeginCombo("FFX 帧生成", currentName.c_str()))
                    {
                        for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                        {
                            auto name = StrFmt("FSR %s", state.ffxFGVersionNames[n]);
                            if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                                _ffxFGIndex = n;
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ShowTooltip("FFX SDK 报告的帧生成器列表");

                    ImGui::SameLine(0.0f, 6.0f);

                    if (ImGui::Button("更换帧生成") && _ffxFGIndex != config->FfxFGIndex.value_or_default())
                    {
                        config->FfxFGIndex = _ffxFGIndex;
                        state.fgChanged = true;
                    }
                }

                bool fgAsync = config->FGAsync.value_or_default();
                if (ImGui::Checkbox("允许异步##2", &fgAsync))
                {
                    config->FGAsync = fgAsync;

                    if (config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("异步已设置 FGChanged");
                    }
                }
                ShowTooltip("启用异步可获得更好的帧生成性能\n可能导致崩溃，尤其在使用 HUD 修复时！");

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                bool fgDV = config->FGDebugView.value_or_default();
                if (ImGui::Checkbox("调试视图##3", &fgDV))
                {
                    config->FGDebugView = fgDV;

                    if (config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("调试视图已设置 FGChanged");
                    }
                }
                ShowTooltip("启用 FSR3.1-FG 调试视图\n\n"
                            "左上：游戏运动矢量\n"
                            "中上：GMV 深度\n"
                            "右上：光流运动矢量\n"
                            "中间：仅插值帧\n"
                            "左下：去遮挡蒙版\n"
                            "中下：插值来源（无 UI）\n"
                            "右下：HUDless 资源");

                if (Nvngx_FG::version().major > 3)
                {
                    ImGui::SameLine(0.0f, 20.0f * menuResScale);
                    if (bool fgwm = config->FSRFGEnableWatermark.value_or_default();
                        ImGui::Checkbox("启用水印", &fgwm))
                    {
                        LOG_DEBUG("FSRFGEnableWatermark 已设置 FGWatermark：{}", fgwm);
                        config->FSRFGEnableWatermark = fgwm;
                        Util::SetFlagEnvVar(L"MLFI-WATERMARK", fgwm);

                        // The driver reads the variable on context creation
                        if (config->IsFGEnabled())
                            state.fgChanged = true;
                    }
                }
            }

            if (bool disableHudless = config->NvngxFGDisableHudless.value_or_default();
                ImGui::Checkbox("禁用 HUDless", &disableHudless))
            {
                config->NvngxFGDisableHudless = disableHudless;
            }
            ShowTooltip("某些 DispatchFlags 组合可能需要此项");
        }
    }

    // FSR-FG Inputs
    if (ScopedCard::Shows("fg_fsrfg_inputs") && state.currentFGSwapchain != nullptr &&
        (state.activeFgInput == FGInput::FSRFG || state.activeFgInput == FGInput::FSRFG30))
    {
        ScopedCard card;

        SeparatorWithHelpMarker("帧生成（FSR-FG 输入）", "请在游戏内选择 FSR-FG");

        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
        if (fgOutput != nullptr)
        {
            ImGui::Text("当前 FSR-FG 状态：");
            ImGui::SameLine();
            if (state.fsrfgInputActive)
            {
                if (fgOutput->IsActive())
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                else
                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "启用帧生成");
            }
            else
            {
                ImGui::TextColored(StatusError(), "关");
                ImGui::Text("请在游戏选项中启用 FSR 帧生成\n"
                            "您可能需要先选择 FSR");
            }
        }

        bool skipConfig = config->FSRFGSkipConfigForHudless.value_or_default();
        if (ImGui::Checkbox("为 HUDless 跳过配置", &skipConfig))
            config->FSRFGSkipConfigForHudless = skipConfig;

        ShowTooltip("不使用 ffxConfig 中设置的 HUDless");

        ImGui::SameLine(0.0f, 6.0f);

        bool skipDispatch = config->FSRFGSkipDispatchForHudless.value_or_default();
        if (ImGui::Checkbox("为 HUDless 跳过 Dispatch", &skipDispatch))
            config->FSRFGSkipDispatchForHudless = skipDispatch;

        ShowTooltip("不使用 ffxDispatch 中设置的 HUDless");
    }

    // Streamline FG Inputs
    if (ScopedCard::Shows("fg_sl_inputs") && state.currentFGSwapchain != nullptr &&
        state.activeFgInput == FGInput::DLSSG)
    {
        ScopedCard card;

        SeparatorWithHelpMarker("帧生成（Streamline FG 输入）", "请在游戏内选择 DLSS-FG");

        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);

        if (!ReflexHooks::isReflexHooked())
        {
            ImGui::TextColored(StatusError(), "Reflex 未挂钩");
            ImGui::Text("如果您使用的是 AMD/Intel 显卡，请确保已安装 fakenvapi");
        }
        else if (fgOutput != nullptr)
        {
            ImGui::Text("当前 Streamline FG 状态：");
            ImGui::SameLine();
            if ((state.fgLastFrame - state.dlssgLastFrame) < 3)
            {
                if (fgOutput->IsActive())
                    ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                else
                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "启用帧生成");
            }
            else
            {
                ImGui::TextColored(StatusError(), "关");
                ImGui::Text("请在游戏选项中启用 DLSS 帧生成\n"
                            "您可能需要先选择 DLSS");
            }
        }
    }
}

void MenuCommon::RenderFsrCommonSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        // FSR Common -----------------
        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            (state.activeFgOutput == FGOutput::FSRFG || IsFsr(currentBackend)))
        {
            SeparatorWithHelpMarker("FSR 通用设置", "同时影响 FSR-FG 与超分器");

            bool useFsrVales = config->FsrUseFsrInputValues.value_or_default();
            if (ImGui::Checkbox("使用 FSR 输入值", &useFsrVales))
                config->FsrUseFsrInputValues = useFsrVales;

            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader("视场角与相机数值"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                bool useVFov = config->FsrVerticalFov.has_value() || !config->FsrHorizontalFov.has_value();

                float vfov = config->FsrVerticalFov.value_or_default();
                float hfov = config->FsrHorizontalFov.value_or(90.0f);

                if (useVFov && !config->FsrVerticalFov.has_value())
                    config->FsrVerticalFov = vfov;
                else if (!useVFov && !config->FsrHorizontalFov.has_value())
                    config->FsrHorizontalFov = hfov;

                if (ImGui::RadioButton("使用垂直视场角", useVFov))
                {
                    config->FsrHorizontalFov.reset();
                    config->FsrVerticalFov = vfov;
                    useVFov = true;
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::RadioButton("使用水平视场角", !useVFov))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov = hfov;
                    useVFov = false;
                }

                if (useVFov)
                {
                    if (ImGui::SliderFloat("垂直视场角", &vfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrVerticalFov = vfov;

                    ShowTooltip("可能有助于获得更好的画质");
                }
                else
                {
                    if (ImGui::SliderFloat("水平视场角", &hfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrHorizontalFov = hfov;

                    ShowTooltip("可能有助于获得更好的画质");
                }

                float cameraNear;
                float cameraFar;

                cameraNear = config->FsrCameraNear.value_or_default();
                cameraFar = config->FsrCameraFar.value_or_default();

                if (ImGui::SliderFloat("相机近平面", &cameraNear, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraNear = cameraNear;
                ShowTooltip("可能有助于获得更好的画质\n"
                            "并可能减少拖影");

                if (ImGui::SliderFloat("相机远平面", &cameraFar, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraFar = cameraFar;
                ShowTooltip("可能有助于获得更好的画质\n"
                            "并可能减少拖影");

                if (ImGui::Button("重置相机数值"))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov.reset();
                    config->FsrCameraNear.reset();
                    config->FsrCameraFar.reset();
                }

                ImGui::SameLine(0.0f, 6.0f);
                ImGui::Text("近：%.1f 远：%.1f",
                            state.lastFsrCameraNear < 500000.0f ? state.lastFsrCameraNear : 500000.0f,
                            state.lastFsrCameraFar < 500000.0f ? state.lastFsrCameraFar : 500000.0f);

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

void MenuCommon::RenderFramerateSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;

    // Framerate ---------------------
    if (state.reflexLimitsFps || config->OverlayMenu.value_or_default())
    {
        SeparatorWithHelpMarker(
            "帧率", "尽可能使用 Reflex\n在 AMD/Intel 显卡上，可使用 Fakenvapi 替代 Reflex");

        static std::string currentMethod {};
        const LowLatencyMode activeOutput = InputCommon::active_output();

        // The Reflex output limits through the driver's Reflex
        if (state.reflexLimitsFps)
        {
            currentMethod = "Reflex";
        }
        else if (InputCommon::can_limit_fps())
        {
            // The game's XeFG with its own XeLL
            if (InputXeLL::IsNative())
                currentMethod = "游戏自带 XeLL";
            else if (activeOutput == LowLatencyMode::AntiLag2)
                currentMethod = "FSR Anti-Lag 2.0";
            else if (activeOutput == LowLatencyMode::LatencyFlex)
                currentMethod = "LatencyFlex";
            else if (activeOutput == LowLatencyMode::AntiLagVk)
                currentMethod = "Vulkan AntiLag";
            else if (activeOutput == LowLatencyMode::Reflex)
                currentMethod = "Reflex";
            else if (activeOutput == LowLatencyMode::FpsLimiter)
                currentMethod = "FPS 限制器";
            else
                currentMethod = "XeLL";
        }
        else
        {
            currentMethod = "回退";
        }

        if (state.rtssReflexInjection)
            currentMethod.append("（RTSS）");

        ImGui::Text("当前方式：%s", currentMethod.c_str());

        if (activeOutput == LowLatencyMode::AntiLag2)
            ShowTooltip("FSR Anti-Lag 2.0 是 AntiLag 2 的新名字\n别问我为什么");

        if (state.reflexShowWarning)
        {
            ImGui::TextColored(StatusError(), "在 FSR FG 下使用 Reflex 的限制会有性能开销");

            ImGui::Spacing();
        }

        // set initial value
        if (std::isinf(_limitFps))
            _limitFps = config->FramerateLimit.value_or_default();

        float fpsSliderMax = (float) std::max(200, refreshRate);
        ImGui::SliderFloat("FPS 上限", &_limitFps, 0, fpsSliderMax, "%.0f");

        if (ImGui::Button("应用上限"))
        {
            config->FramerateLimit = _limitFps;
        }

        ImGui::SameLine(0.0f, 16.0f);

        if (ImGui::Button("重置上限"))
        {
            _limitFps = 0.0f;
            config->FramerateLimit = _limitFps;
        }

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader("VRR 帧率上限计算器"); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            ImGui::PushItemWidth(105.0f * menuResScale);
            ImGui::InputInt("刷新率", &refreshRate, 1, 1, ImGuiInputTextFlags_None);
            ImGui::PopItemWidth();

            float refreshRateF = static_cast<float>(refreshRate);
            // it's fine to use with real reflex, we only care about antilag
            auto fpsLimitTech = InputCommon::active_output();
            constexpr float margin = 0.3f; // in ms
            float frameCap = std::round(10000.f / (1000.f / refreshRateF + margin)) / 10.f;

            if (fpsLimitTech == LowLatencyMode::AntiLag2 || fpsLimitTech == LowLatencyMode::AntiLagVk)
                frameCap = std::round(frameCap);

            ImGui::Text("计算出的上限：%.1f", frameCap);

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("设为 FPS 上限"))
            {
                _limitFps = frameCap;
                config->FramerateLimit = _limitFps;
            }
        }
    }
}

template <typename T> std::string GetMenuOptionLabel(const std::vector<MenuOption<T>>& options, T targetValue)
{
    auto it = std::find_if(options.begin(), options.end(),
                           [targetValue](const MenuOption<T>& option) { return option.value == targetValue; });

    if (it != options.end())
    {
        return it->label;
    }

    return "未知";
}

void MenuCommon::RenderLowLatencySettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Low Latency ---------------------------
    SectionTitle("低延迟");

    static std::vector<MenuOption<LowLatencyInput>> lowLatencyInput = {
        { LowLatencyInput::None, "无" },
        { LowLatencyInput::Auto, "默认" },
        { LowLatencyInput::AntiLag2, "FSR Anti-Lag 2.0" },
        { LowLatencyInput::Reflex, "Reflex" },
        { LowLatencyInput::XeLL, "XeLL" },
        { LowLatencyInput::UeLowLatency, "UeLowLatency" },
        { LowLatencyInput::OptiScaler, "OptiScaler" },
    };

    static std::vector<MenuOption<LowLatencyMode>> lowLatencyOutput = {
        { LowLatencyMode::None, "无" },
        { LowLatencyMode::Auto, "自动" },
        { LowLatencyMode::LatencyFlex, "LatencyFlex" },
        { LowLatencyMode::AntiLag2, "FSR Anti-Lag 2.0" },
        { LowLatencyMode::XeLL, "XeLL" },
        { LowLatencyMode::AntiLagVk, "AntiLag Vk" },
        { LowLatencyMode::Reflex, "Reflex" },
        { LowLatencyMode::FpsLimiter, "FPS 限制器", "仅限制 FPS" },
    };

    LowLatencyInput activeInput {};
    LowLatencyMode activeOutput {};

    if (ImGui::BeginTable("lowLatencyActive", 2, ImGuiTableFlags_SizingStretchSame))
    {
        InputCommon::get_currently_active(activeInput, activeOutput);

        ImGui::TableNextColumn();

        ImGui::Text("当前输入：%s", GetMenuOptionLabel(lowLatencyInput, activeInput).c_str());

        ImGui::TableNextColumn();

        ImGui::Text("当前输出：%s", GetMenuOptionLabel(lowLatencyOutput, activeOutput).c_str());

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("lowLatencySelection", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        auto avalibleInputs = InputCommon::get_avaliable_inputs();

        // Inputs become available as the game starts sending them
        for (auto type : { LowLatencyInput::AntiLag2, LowLatencyInput::Reflex, LowLatencyInput::XeLL,
                           LowLatencyInput::UeLowLatency })
        {
            auto& option = lowLatencyInput[(uint32_t) type];
            option.disabled = !avalibleInputs[type];
            option.tooltip = option.disabled ? "游戏未发送此数据" : "";
        }

        auto defaultInput = InputCommon::default_input();
        lowLatencyInput[(uint32_t) LowLatencyInput::None].set_hidden(true);

        // Only the default when the game has nothing of its own
        lowLatencyInput[(uint32_t) LowLatencyInput::OptiScaler].set_hidden(true);
        lowLatencyInput[(uint32_t) LowLatencyInput::Auto].label = "自动";
        lowLatencyInput[(uint32_t) LowLatencyInput::Auto].tooltip =
            "游戏发送的最佳输入：Reflex、游戏中启用时的 XeLL、AntiLag 2、UE、XeLL。\n"
            "当游戏的 FSR-FG 通过 AntiLag 2 报告帧时，优先使用 AntiLag 2。\n"
            "若都没有，则使用 OptiScaler 自带的方案配合 DLSSG 输出。";

        auto forcedInput = InputCommon::forced_input();
        auto input = forcedInput.value_or(config->LowLatencyInput.value_or_default());

        if (input == LowLatencyInput::None)
            input = LowLatencyInput::Auto;

        auto selectedInput = input;

        ImGui::BeginDisabled(forcedInput.has_value());
        PopulateCombo("输入", selectedInput, lowLatencyInput);
        ImGui::EndDisabled();

        if (forcedInput.has_value() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("由 XeFG 设置：游戏自身的保持其 XeLL");
        else if (selectedInput != input)
            config->LowLatencyInput = selectedInput;

        ImGui::TableNextColumn();

        lowLatencyOutput[(uint32_t) LowLatencyMode::None].set_hidden(true);

        // Each API has its own, the other API's selection maps to it. The API can change, so no sticky setters.
        const auto api = InputCommon::uses_api();
        const bool vulkan = api == API::Vulkan;
        const bool outputForced = InputCommon::forced_output().has_value();

        // OptiScaler's XeFG paces with XeLL on D3D12 (also for D3D11 games through interop), the GPU's Reflex or
        // AntiLag 2 output can sleep instead while XeFG keeps an XeLL context of its own
        const bool optiXefg = state.activeFgOutput == FGOutput::XeFG;

        lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLag2].hidden = vulkan;
        lowLatencyOutput[(uint32_t) LowLatencyMode::XeLL].hidden =
            !outputForced && !optiXefg && (vulkan || api == API::DX11);
        lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLagVk].hidden = !vulkan;

        auto disable = [](MenuOption<LowLatencyMode>& option, bool condition, const char* reason)
        {
            option.disabled = condition;
            option.tooltip = condition ? reason : "";
        };

        for (auto mode : { LowLatencyMode::Reflex, LowLatencyMode::AntiLag2, LowLatencyMode::AntiLagVk })
        {
            auto reason = InputCommon::output_unavailable(mode, api);
            disable(lowLatencyOutput[(uint32_t) mode], reason != nullptr, reason);
        }

        disable(lowLatencyOutput[(uint32_t) LowLatencyMode::LatencyFlex], optiXefg, "不适用于 XeFG");

        lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].label = "自动";
        lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].tooltip =
            vulkan ? "GPU 自带方案：AMD 用 AntiLag，Nvidia 用 Reflex，其他用 LatencyFlex"
            : api == API::DX11
                ? "GPU 自带方案：AMD 用 AntiLag 2，Nvidia 用 Reflex，其他用 LatencyFlex"
                : "GPU 自带方案：AMD 用 AntiLag 2，Nvidia 用 Reflex，Intel 用 XeLL，其他用 LatencyFlex";

        if (optiXefg)
        {
            auto& autoTooltip = lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].tooltip;
            autoTooltip = "XeLL，XeFG 用它来节流。";

            if (!lowLatencyOutput[(uint32_t) LowLatencyMode::Reflex].disabled)
                autoTooltip += "\nReflex 可以代为休眠，XeFG 会保留自己的 XeLL 上下文。";
            else if (!lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLag2].disabled)
                autoTooltip += "\nAntiLag 2 可以代为休眠，XeFG 会保留自己的 XeLL 上下文。";
        }

        // Frame generation decides the output it works with
        auto forcedOutput = InputCommon::forced_output();
        auto output = forcedOutput.value_or(config->LowLatencyOutput.value_or_default());

        // An output the GPU can't run works as Auto
        if (output == LowLatencyMode::None || InputCommon::output_unavailable(output, api))
            output = LowLatencyMode::Auto;

        // The other API's AntiLag
        if (vulkan && output == LowLatencyMode::AntiLag2)
            output = LowLatencyMode::AntiLagVk;
        else if (!vulkan && output == LowLatencyMode::AntiLagVk)
            output = LowLatencyMode::AntiLag2;

        // No XeLL on D3D11 unless OptiScaler's XeFG runs it on D3D12, Auto picks the GPU's own
        if (!forcedOutput.has_value() && !optiXefg && api == API::DX11 && output == LowLatencyMode::XeLL)
            output = LowLatencyMode::Auto;

        auto selectedOutput = output;

        ImGui::BeginDisabled(forcedOutput.has_value());
        PopulateCombo("输出", selectedOutput, lowLatencyOutput);
        ImGui::EndDisabled();

        if (forcedOutput.has_value() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("由 XeFG 设置：游戏自身的保持其 XeLL（仅 FPS 限制由 OptiScaler 控制）");
        else if (selectedOutput != output)
            config->LowLatencyOutput = selectedOutput;

        ImGui::EndTable();
    }

    if (InputCommon::xefg_output_change_pending())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped("保存设置并重启游戏：XeFG 会保留启动时的低延迟输出");
        ImGui::PopStyleColor();
    }

    if (auto reason = InputCommon::incompatibility(activeInput, activeOutput))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped("不兼容：%s", reason);
        ImGui::PopStyleColor();
    }

    if (activeOutput == LowLatencyMode::LatencyFlex)
    {
        static const std::vector<MenuOption<LFXMode>> lfx_modes = {
            { LFXMode::Conservative, "保守", "最安全，但可能无法很好地降低延迟" },
            { LFXMode::Aggressive, "激进",
              "改善延迟，但在某些情况下 FPS 下降可能超出预期" },
            { LFXMode::ReflexIDs, "Reflex ID",
              "可用时效果最佳，部分游戏不兼容（例如《赛博朋克》)\n"
              "并将回退到激进模式" }
        };

        PopulateCombo("LatencyFlex 模式", config->FN_LatencyFlexMode, lfx_modes);
    }

    static std::vector<MenuOption<ForceReflex>> lowlatency_states = { { ForceReflex::InGame, "跟随游戏设置" },
                                                                      { ForceReflex::ForceDisable, "强制禁用" },
                                                                      { ForceReflex::ForceEnable, "强制启用" } };

    ImGui::SetNextItemWidth(150.0f * ctx.menuResScale);
    PopulateCombo("强制状态", config->FN_ForceReflex, lowlatency_states);
}

void MenuCommon::RenderActiveImageSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    bool rcasEnabled = false;

    // SHARPNESS -----------------------------
    SectionTitle("锐化");

    if (bool overrideSharpness = config->OverrideSharpness.value_or_default();
        ImGui::Checkbox("覆盖", &overrideSharpness))
    {
        config->OverrideSharpness = overrideSharpness;

        if ((currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12) &&
            currentFeature->Version().major < 3)
        {
            state.newBackend = SameBackend(currentBackend);
            MARK_ALL_BACKENDS_CHANGED();
        }
    }
    ShowTooltip("忽略游戏发送的值\n"
                "并使用下方设置的值");

    ImGui::SameLine(0.0f, 16.0f * menuResScale);

    float featuresCurrentSharpness = currentFeature->Sharpness();
    if (featuresCurrentSharpness > 0.0f)
        ImGui::TextDisabled("（当前：%.3f）", featuresCurrentSharpness);
    else
        ImGui::TextDisabled("（当前：已禁用）");

    ImGui::BeginDisabled(!config->OverrideSharpness.value_or_default());

    float sharpness = config->Sharpness.value_or_default();

    if (ImGui::SliderFloat("强度", &sharpness, 0.0f, 1.0f))
        config->Sharpness = sharpness;

    ImGui::EndDisabled();

    // RCAS
    {
        // xess or dlss version >= 2.5.1
        constexpr feature_version requiredDlssVersion = { 2, 5, 1 };
        const bool isDlss = currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12;
        rcasEnabled = currentBackend == Upscaler::XeSS || (isDlss && currentFeature->Version() >= requiredDlssVersion);

        // Determine current mode for the dropdown
        bool currentRcasEnabled = config->RcasEnabled.value_or(rcasEnabled);
        auto currentShader = Config::Instance()->SharpnessShader.value_or_default();

        int selectedMode = 0; // 0 = Upscaler's Default
        if (currentRcasEnabled)
        {
            if (currentShader == SharpenShader::RCAS)
                selectedMode = 1;
            else if (currentShader == SharpenShader::DepthAware)
                selectedMode = 2;
            else if (currentShader == SharpenShader::LocalContrastDepthAware)
                selectedMode = 3;
        }

        const char* sharpenModes[] = { "超分器默认", "RCAS", "深度感知 (RCAS)", "深度感知 (DAS)" };

        if (ImGui::Combo("方法", &selectedMode, sharpenModes, IM_ARRAYSIZE(sharpenModes)))
        {
            if (selectedMode == 0)
            {
                config->RcasEnabled = false;
            }
            else
            {
                config->RcasEnabled = true;
                if (selectedMode == 1)
                    Config::Instance()->SharpnessShader = SharpenShader::RCAS;
                else if (selectedMode == 2)
                    Config::Instance()->SharpnessShader = SharpenShader::DepthAware;
                else if (selectedMode == 3)
                    Config::Instance()->SharpnessShader = SharpenShader::LocalContrastDepthAware;
            }
        }

        ShowTooltip("选择要应用的锐化滤镜。\n\n"
                    "超分器默认：使用超分器提供的锐化。\n"
                    "RCAS：AMD 的 RCAS，针对对比度与 MAS 进行了修改。\n"
                    "深度感知：更智能的锐化，伪影更少（开销更高）。\n\n"
                    "默认使用游戏提供的锐化值。\n"
                    "在“锐化”下选择“覆盖”，然后调整滑块进行更改。");

        // Only show MAS and Advanced parameters if a custom sharpness mode is active
        if (selectedMode != 0)
        {
            ImGui::Spacing();

            if (bool overrideMotionSharpness = config->MotionSharpnessEnabled.value_or_default();
                ImGui::Checkbox("启用运动自适应锐化", &overrideMotionSharpness))
                config->MotionSharpnessEnabled = overrideMotionSharpness;
            ShowTooltip("根据运动启用锐化调整");

            if (Config::Instance()->SharpnessShader.value_or_default() != SharpenShader::RCAS)
            {
                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                    ImGui::Checkbox("DA + MAS 调试", &overrideMSDebug))
                    config->MotionSharpnessDebug = overrideMSDebug;

                ShowTooltip("启用 DA + MAS 调试视图\n"
                            "检测到的 DA 边缘显示蓝色\n\n"
                            "红色区域越多表示应用的锐化越强\n"
                            "绿色区域将减少锐化");

                if (auto ch = ScopedCollapsingHeader("高级深度感知参数"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (bool clamp = config->DAClampOutput.value_or(false); ImGui::Checkbox("钳制输出", &clamp))
                    {
                        if (clamp)
                            config->DAClampOutput = true;
                        else
                            config->DAClampOutput.reset();
                    }

                    ShowTooltip("将最终图像钳制到 [0, 1] 范围。\n\n"
                                "防止过冲伪影，如明亮光晕或负色。\n"
                                "推荐用于 LDR 管线；HDR 可依据色调映射选择开启。\n\n"
                                "未设置时，OptiScaler 通过超分器的 HDR 标志控制");

                    if (currentFeature->DepthLinear())
                    {
                        float depthBias = config->DADepthBias.value_or(0.0015f);
                        if (ImGui::SliderFloat("深度偏差", &depthBias, 0.005f, 0.03f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowTooltip("在边缘检测前忽略微小的深度差异。\n\n"
                                    "较高的值减少轻微深度变化引起的闪烁和噪点，但可能"
                                    "软化真实的几何边缘。\n"
                                    "较低的值保留精细细节，但可能导致边缘"
                                    "检测不稳定或嘈杂。");

                        float depthScale = config->DADepthScale.value_or(250.0f);
                        if (ImGui::SliderFloat("深度缩放", &depthScale, 100.0f, 600.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowTooltip("控制锐化在深度边缘处被减弱的强度。\n\n"
                                    "较高的值更积极地防止锐化跨越物体边界"
                                    "（减少光晕）。\n"
                                    "较低的值允许更多锐化穿过边缘（更锐利但"
                                    "风险更高）。");
                    }
                    else
                    {
                        float depthBias = config->DADepthBias.value_or(0.001f);
                        if (ImGui::SliderFloat("深度偏差", &depthBias, 0.0001f, 0.003f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowTooltip("在边缘检测前忽略微小的深度差异。\n\n"
                                    "较高的值减少轻微深度变化引起的闪烁和噪点，但可能"
                                    "软化真实的几何边缘。\n"
                                    "较低的值保留精细细节，但可能导致边缘"
                                    "检测不稳定或嘈杂。");

                        float depthScale = config->DADepthScale.value_or(35.0f);
                        if (ImGui::SliderFloat("深度缩放", &depthScale, 25.0f, 400.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowTooltip("控制锐化在深度边缘处被减弱的强度。\n\n"
                                    "较高的值更积极地防止锐化跨越物体边界"
                                    "（减少光晕）。\n"
                                    "较低的值允许更多锐化穿过边缘（更锐利但"
                                    "风险更高）。");
                    }

                    if (ImGui::Button("重置深度数值"))
                    {
                        config->DADepthBias.reset();
                        config->DADepthScale.reset();
                    }
                }
            }
            else
            {
                if (bool contrastEnabled = config->ContrastEnabled.value_or_default();
                    ImGui::Checkbox("启用对比度", &contrastEnabled))
                    config->ContrastEnabled = contrastEnabled;

                ShowTooltip("控制高对比度区域的锐化。");

                ImGui::BeginDisabled(!config->ContrastEnabled.value_or_default());

                float contrast = config->Contrast.value_or_default();
                if (ImGui::SliderFloat("对比度", &contrast, 0.0f, 2.0f, "%.2f"))
                    config->Contrast = contrast;

                ShowTooltip("降低高对比区域的锐度。");

                ImGui::EndDisabled();
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader("运动自适应锐化##2"); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                ImGui::BeginDisabled(!config->MotionSharpnessEnabled.value_or_default());

                if (Config::Instance()->SharpnessShader.value_or_default() == SharpenShader::RCAS)
                {
                    if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                        ImGui::Checkbox("MAS 调试", &overrideMSDebug))
                        config->MotionSharpnessDebug = overrideMSDebug;
                    ShowTooltip("越红的区域将应用更多锐化\n"
                                "绿色区域将减少锐化");
                }

                float motionSharpness = config->MotionSharpness.value_or_default();
                if (ImGui::SliderFloat("运动锐化强度", &motionSharpness, -1.0f, 1.0f, "%.3f"))
                    config->MotionSharpness = motionSharpness;

                ShowTooltip("运动可增加或减少的最大锐化量。\n\n"
                            "负值减少运动中的锐化（推荐）。\n"
                            "正值增加运动中的锐化。\n\n"
                            "最终调整随运动缩放，并以该值为上限。");

                float motionThreshod = config->MotionThreshold.value_or_default();
                if (ImGui::SliderFloat("运动阈值", &motionThreshod, 0.0f, 100.0f, "%.2f"))
                    config->MotionThreshold = motionThreshod;

                ShowTooltip("开始基于运动的锐化调整所需的最小运动量。\n\n"
                            "较高的值忽略微小运动（更稳定）。\n"
                            "较低的值对细微运动更敏感。");

                float motionScale = config->MotionScaleLimit.value_or_default();
                if (ImGui::SliderFloat("运动范围", &motionScale, 0.01f, 100.0f, "%.2f"))
                    config->MotionScaleLimit = motionScale;

                ShowTooltip("定义效果从零到全强度的运动范围。\n\n"
                            "高于阈值的值映射到此范围。\n"
                            "较大的值使响应更平滑、更渐进。\n"
                            "较小的值使效果反应更快、更激进。");

                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

void MenuCommon::RenderUpscaleRatioSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // UPSCALE RATIO OVERRIDE -----------------

    auto minSliderLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
    auto maxSliderLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;

    SectionTitle("超分倍率覆盖");

    if (ImGui::BeginTable("ratioOverride", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextColumn();

        if (bool upOverride = config->UpscaleRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox("覆盖全部", &upOverride))
        {
            config->UpscaleRatioOverrideEnabled = upOverride;

            if (upOverride)
                config->QualityRatioOverrideEnabled = false;
        }
        ShowTooltip("用设置的值覆盖所有超分器预设\n\n"
                    "在 1080p 屏幕上 1.5x 意味着内部分辨率为 720p\n"
                    "1080 / 1.5 = 720");

        ImGui::TableNextColumn();

        if (bool qOverride = config->QualityRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox("按画质预设分别覆盖", &qOverride))
        {
            config->QualityRatioOverrideEnabled = qOverride;

            if (qOverride)
                config->UpscaleRatioOverrideEnabled = false;
        }

        ShowTooltip("允许您单独覆盖每个预设的倍率\n"
                    "请注意，并非每个游戏都支持所有画质预设\n\n"
                    "在 1080p 屏幕上 1.5x 意味着内部分辨率为 720p\n"
                    "1080 / 1.5 = 720");

        ImGui::EndTable();
    }

    if (config->UpscaleRatioOverrideEnabled.value_or_default())
    {
        float urOverride = config->UpscaleRatioOverrideValue.value_or_default();
        ImGui::SliderFloat("所有倍率", &urOverride, minSliderLimit, maxSliderLimit, "%.3f");
        config->UpscaleRatioOverrideValue = urOverride;
    }

    if (config->QualityRatioOverrideEnabled.value_or_default())
    {
        float qDlaa = config->QualityRatio_DLAA.value_or_default();
        if (ImGui::SliderFloat("DLAA", &qDlaa, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_DLAA = qDlaa;

        float qUq = config->QualityRatio_UltraQuality.value_or_default();
        if (ImGui::SliderFloat("极致质量", &qUq, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_UltraQuality = qUq;

        float qQ = config->QualityRatio_Quality.value_or_default();
        if (ImGui::SliderFloat("质量", &qQ, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Quality = qQ;

        float qB = config->QualityRatio_Balanced.value_or_default();
        if (ImGui::SliderFloat("均衡", &qB, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Balanced = qB;

        float qP = config->QualityRatio_Performance.value_or_default();
        if (ImGui::SliderFloat("性能", &qP, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Performance = qP;

        float qUp = config->QualityRatio_UltraPerformance.value_or_default();
        if (ImGui::SliderFloat("极致性能", &qUp, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_UltraPerformance = qUp;
    }
}

void MenuCommon::RenderOutputScalingSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // OUTPUT SCALING -----------------------------
    // if (state.api == DX12 || state.api == DX11)
    {
        // if motion vectors are not display size
        ImGui::BeginDisabled(!currentFeature->LowResMV() &&
                             currentFeature->RenderWidth() != currentFeature->DisplayWidth());

        SectionTitle("输出缩放");

        float defaultRatio = 1.5f;

        if (_ssRatio == 0.0f)
        {
            _ssRatio = config->OutputScalingMultiplier.value_or(defaultRatio);
            _ssEnabled = config->OutputScalingEnabled.value_or_default();
            _ssDownsampler = config->OutputScalingDownscaler.value_or_default();
        }

        ImGui::BeginDisabled((currentBackend == Upscaler::XeSS || currentBackend == Upscaler::DLSS ||
                              currentBackend == Upscaler::DLSS_on12) &&
                             currentFeature->RenderWidth() > currentFeature->DisplayWidth());
        ImGui::Checkbox("启用", &_ssEnabled);
        ImGui::EndDisabled();

        ShowTooltip("在内部将图像放大到更高的输出分辨率\n"
                    "然后将其缩小回您的显示分辨率\n\n"
                    "值 <1.0 使超分器负载更低\n"
                    "值 >1.0 以性能为代价使图像更锐利\n\n"
                    "如果呈灰色，请查看 Git Wiki - 虚幻引擎调整\n\n"
                    "底部显示目标分辨率和总倍率（最大总计 3.0！）");

        ImGui::SameLine(0.0f, 6.0f);

        ImGui::BeginDisabled(!_ssEnabled);
        {
            ImGui::PushItemWidth(105.0f * menuResScale);

            // clang-format off
                    std::vector<MenuOption<Scaler>> ds_options = {
                        { Scaler::FSR1, "FSR1",
                            "默认选项。\n画质不错且速度极快。" },
                        { Scaler::Bicubic, "Bicubic",
                            "最快的传统选项。\n图像非常柔和/模糊，但用于降采样可能还行。" },
                        { Scaler::CatmullRom, "Catmull-Rom",
                            "主要为降采样设计。\n保留良好对比度且伪影极少，但比 Lanczos 更柔和。" },
                        { Scaler::Lanczos2, "Lanczos2",
                            "比 Lanczos3 更轻更快。\n振铃伪影更少，但略微更模糊。" },
                        { Scaler::Lanczos3, "Lanczos3",
                            "Lanczos2 的加强版。\n画面最锐利，但最容易出现振铃。\n与 Kaiser3 并称最佳。" },
                        { Scaler::Kaiser2, "Kaiser2",
                            "与 Lanczos2 类似。\n比 Lanczos 更平滑、伪影更少，但略微模糊。" },
                        { Scaler::Kaiser3, "Kaiser3",
                            "与 Lanczos3 类似。\n比 Lanczos3 更不易产生伪影，但对 GPU 的负担大得多。\n与 Lanczos3 并称最佳。" },
                        { Scaler::Magic, "MAGIC",
                            "专为防止伪影而设计。\n消除生硬光晕以获得自然观感，但可能略显柔和。" }
                    };
            // clang-format on

            const bool isUpsampleRatio = _ssRatio < 1.0f;
            const std::string disabledReason = "当倍率低于 1.0 时，仅支持 FSR1 和 Bicubic。";

            for (auto& opt : ds_options)
            {
                if (isUpsampleRatio && opt.value > Scaler::Bicubic)
                    opt.set_disabled(true, opt.tooltip + "\n\n" + disabledReason);
            }

            if (isUpsampleRatio && _ssDownsampler > Scaler::Bicubic)
                _ssDownsampler = Scaler::FSR1;

            PopulateCombo("降采样器", _ssDownsampler, ds_options);

            ImGui::PopItemWidth();
        }
        ImGui::EndDisabled();

        bool applyEnabled = _ssEnabled != config->OutputScalingEnabled.value_or_default() ||
                            _ssRatio != config->OutputScalingMultiplier.value_or(defaultRatio) ||
                            _ssDownsampler != config->OutputScalingDownscaler.value_or_default();

        ImGui::BeginDisabled(!applyEnabled);
        if (ImGui::Button("应用更改"))
        {
            config->OutputScalingEnabled = _ssEnabled;
            config->OutputScalingMultiplier = _ssRatio;

            if (_ssRatio < 1.0f && _ssDownsampler > Scaler::Bicubic)
                _ssDownsampler = Scaler::FSR1;

            config->OutputScalingDownscaler = _ssDownsampler;

            // Ray reconstruction keeps its DLSSD, the upscalers their configured one
            if (state.rrHandles.contains(currentFeature->Handle()->Id))
                state.newBackend = Upscaler::Reset;
            else if (currentFeature->GetUpscalerType() == Upscaler::DLSSD)
                state.newBackend = Upscaler::DLSSD;
            else
                state.newBackend = SameBackend(currentBackend);

            MARK_ALL_BACKENDS_CHANGED();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!_ssEnabled || currentFeature->RenderWidth() > currentFeature->DisplayWidth());
        ImGui::SliderFloat("倍率", &_ssRatio, 0.5f, 3.0f, "%.2f");
        ImGui::EndDisabled();

        if (currentFeature != nullptr && !currentFeature->IsFrozen())
        {
            ImGui::Text("输出缩放：%s，目标分辨率：%dx%d（%.2f）\n抖动采样数：%d",
                        config->OutputScalingEnabled.value_or_default() ? "已启用" : "已禁用",
                        (uint32_t) (currentFeature->DisplayWidth() * _ssRatio),
                        (uint32_t) (currentFeature->DisplayHeight() * _ssRatio),
                        ((float) currentFeature->DisplayWidth() * _ssRatio) / (float) currentFeature->RenderWidth(),
                        currentFeature->JitterCount());
        }

        ImGui::EndDisabled();
    }
}

void MenuCommon::RenderInitFlagsSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    if (currentFeature == nullptr || currentFeature->IsFrozen())
        return;

    // INIT -----------------------------
    SectionTitle("初始化标志");

    if (ImGui::BeginTable("init", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        // AutoExposure is always enabled for XeSS with native Dx11
        bool autoExposureDisabled = state.api == API::DX11 && currentBackend == Upscaler::XeSS;
        ImGui::BeginDisabled(autoExposureDisabled);

        if (bool autoExposure = currentFeature->AutoExposure(); ImGui::Checkbox("自动曝光", &autoExposure))
        {
            config->AutoExposure = autoExposure;
            ReInitUpscaler();
        }
        ShowTooltip("某些虚幻引擎游戏需要此项\n\n"
                    "如果颜色闪烁或\n"
                    "物体有拖影，请尝试");
        ShowResetButton(&config->AutoExposure, "R");

        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        if (bool depth = currentFeature->DepthInverted(); ImGui::Checkbox("深度反转", &depth))
        {
            config->DepthInverted = depth;
            ReInitUpscaler();
        }
        ShowTooltip("您通常不需要更改它");
        ShowResetButton(&config->DepthInverted, "重置##2");

        ImGui::TableNextColumn();
        if (bool hdr = currentFeature->IsHdr(); ImGui::Checkbox("HDR", &hdr))
        {
            config->HDR = hdr;
            ReInitUpscaler();
        }
        ShowTooltip("可能有助于解决部分游戏中的紫色色调问题");
        ShowResetButton(&config->HDR, "重置##1");

        ImGui::TableNextColumn();
        if (bool jitter = currentFeature->JitteredMV(); ImGui::Checkbox("抖动消除", &jitter))
        {
            config->JitterCancellation = jitter;
            ReInitUpscaler();
        }
        ShowTooltip("修复游戏发送已预应用抖动的运动数据的问题");
        ShowResetButton(&config->JitterCancellation, "重置##3");

        ImGui::TableNextColumn();
        if (bool mv = !currentFeature->LowResMV(); ImGui::Checkbox("显示分辨率运动矢量", &mv))
        {
            config->DisplayResolution = mv;

            // Disable output scaling when
            // Display res MV is active
            if (mv)
            {
                config->OutputScalingEnabled = false;
                _ssEnabled = false;
            }

            ReInitUpscaler();
        }
        ShowTooltip("主要用于修复虚幻引擎游戏\n"
                    "屏幕左上部分会变模糊");
        ShowResetButton(&config->DisplayResolution, "重置##4");

        ImGui::TableNextColumn();
        ImGui::BeginDisabled(currentBackend != Upscaler::FFX && currentBackend != Upscaler::FFX_on12);

        if (bool drs = currentFeature->DynamicResolution(); ImGui::Checkbox("动态分辨率（DRS）", &drs))
        {
            config->DynamicResolution = drs;
            ReInitUpscaler();
        }
        ShowTooltip("若游戏使用动态分辨率则启用\n"
                    "防止 FSR 4 在渲染尺寸变化时切换模型");
        ShowResetButton(&config->DynamicResolution, "R##5");

        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        auto accessToReactiveMask = currentFeature->AccessToReactiveMask();
        ImGui::BeginDisabled(!accessToReactiveMask);

        bool canUseReactiveMask =
            accessToReactiveMask && currentBackend != Upscaler::DLSS && currentBackend != Upscaler::DLSS_on12 &&
            (currentBackend != Upscaler::XeSS || currentFeature->Version() >= feature_version { 2, 0, 1 });

        bool disableReactiveMask = config->DisableReactiveMask.value_or(!canUseReactiveMask);

        if (ImGui::Checkbox("禁用反应蒙版", &disableReactiveMask))
        {
            config->DisableReactiveMask = disableReactiveMask;

            if (currentBackend == Upscaler::XeSS)
            {
                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
        }

        ImGui::EndDisabled();

        if (accessToReactiveMask)
            ShowTooltip("允许使用反应蒙版\n"
                        "请注意，发送给 DLSS 的反应蒙版\n"
                        "与 FSR/XeSS 结合使用时不会产生好的图像");
        else
            ShowTooltip("此选项已禁用，因为游戏不提供反应蒙版");

        ImGui::EndTable();
    }

    if (currentFeature->AccessToReactiveMask())
    {
        ImGui::BeginDisabled(config->DisableReactiveMask.value_or(false));

        auto useAsTransparency = config->FsrUseMaskForTransparency.value_or_default();
        if (ImGui::Checkbox("将反应蒙版用作透明度蒙版", &useAsTransparency))
            config->FsrUseMaskForTransparency = useAsTransparency;

        ImGui::EndDisabled();
    }

    if (currentFeature->AccessToReactiveMask() && currentBackend != Upscaler::DLSS &&
        currentBackend != Upscaler::DLSS_on12)
    {
        ImGui::BeginDisabled(config->DisableReactiveMask.value_or(currentBackend == Upscaler::XeSS));

        bool binaryMask = state.api == Vulkan || currentBackend == Upscaler::XeSS;
        auto defaultBias = binaryMask ? 0.0f : 0.45f;
        auto maskBias = config->DlssReactiveMaskBias.value_or(defaultBias);

        if (!binaryMask)
        {
            if (ImGui::SliderFloat("反应蒙版偏移", &maskBias, 0.0f, 0.9f, "%.2f"))
                config->DlssReactiveMaskBias = maskBias;

            ShowTooltip("高于 0 的值将启用反应蒙版的使用");
        }
        else
        {
            bool useRM = maskBias > 0.0f;
            if (ImGui::Checkbox("使用二值反应蒙版", &useRM))
            {
                if (useRM)
                    config->DlssReactiveMaskBias = 0.45f;
                else
                    config->DlssReactiveMaskBias.reset();
            }
        }

        ImGui::EndDisabled();
    }
}

void MenuCommon::RenderMagnifierSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Magnifier -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("放大镜"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool magnifierEnabled = config->MagnifierEnabled.value_or_default();
        if (ImGui::Checkbox("启用放大镜", &magnifierEnabled))
            config->MagnifierEnabled = magnifierEnabled;

        ImGui::BeginDisabled(!magnifierEnabled);

        float magnifierSize = config->MagnifierSize.value_or_default();
        if (ImGui::SliderFloat("大小", &magnifierSize, 5.0f, 50.0f, "%.1f%% 屏幕"))
            config->MagnifierSize = magnifierSize;

        int zoomFactor = config->MagnifierZoomFactor.value_or_default();
        if (ImGui::SliderInt("缩放倍数", &zoomFactor, 2, 20, "%dx"))
            config->MagnifierZoomFactor = zoomFactor;

        float borderSize = config->MagnifierBorderSize.value_or_default();
        if (ImGui::SliderFloat("边框大小", &borderSize, 0.0f, 2.0f, "%.2f%% 屏幕"))
            config->MagnifierBorderSize = borderSize;

        ImGui::Separator();
        ImGui::Text("定位方式");

        bool staticMode = config->MagnifierStaticPosX.has_value() && config->MagnifierStaticPosY.has_value();
        if (staticMode)
        {
            float staticX = config->MagnifierStaticPosX.value();
            if (ImGui::SliderFloat("静态位置 X", &staticX, 0.0f, 100.0f, "%.1f%%"))
                config->MagnifierStaticPosX = staticX;

            float staticY = config->MagnifierStaticPosY.value();
            if (ImGui::SliderFloat("静态位置 Y", &staticY, 0.0f, 100.0f, "%.1f%%"))
                config->MagnifierStaticPosY = staticY;

            if (ImGui::Button("重置静态位置（跟随光标）"))
            {
                config->MagnifierStaticPosX.reset();
                config->MagnifierStaticPosY.reset();
            }
        }
        else
        {
            // Button to initialize static position mode
            if (ImGui::Button("设置静态位置"))
            {
                config->MagnifierStaticPosX = 50.0f;
                config->MagnifierStaticPosY = 50.0f;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("（当前跟随光标）");

            float offsetX = config->MagnifierCursorOffsetX.value_or_default();
            if (ImGui::SliderFloat("光标偏移 X", &offsetX, -300.0f, 300.0f, "%.0f px"))
                config->MagnifierCursorOffsetX = offsetX;

            float offsetY = config->MagnifierCursorOffsetY.value_or_default();
            if (ImGui::SliderFloat("光标偏移 Y", &offsetY, -300.0f, 300.0f, "%.0f px"))
                config->MagnifierCursorOffsetY = offsetY;
        }

        ImGui::EndDisabled();
        ImGui::Spacing();
    }
}

// List loaded ASI plugins, with their version when the file has one
static std::vector<std::string> ListedAsiPlugins(State& state)
{
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(state.loadedAsiPluginsMutex);
    for (const auto& plugin : state.loadedAsiPlugins)
    {
        if (plugin.version.empty())
            names.push_back(plugin.name);
        else
            names.push_back(std::format("{} ({})", plugin.name, plugin.version));
    }
    return names;
}

// Modular Quirks / ASI tab
static bool QuirksTabVisible(State& state) { return !state.detectedQuirks.empty() || !ListedAsiPlugins(state).empty(); }
static const char* QuirksTabLabel(State& state)
{
    const bool quirks = !state.detectedQuirks.empty();
    const bool asi = !ListedAsiPlugins(state).empty();
    if (quirks && asi)
        return "兼容性修正与 ASI";
    return asi ? "ASI" : "兼容性修正";
}

void MenuCommon::RenderQuirksSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;

    const auto asiPlugins = ListedAsiPlugins(state);

    // QUIRKS -----------------------------
    if (ScopedCard::Shows("active_quirks") && state.detectedQuirks.size() > 0)
    {
        ScopedCard card { "生效的兼容性修正" };

        for (const auto& quirk : state.detectedQuirks)
        {
            ImGui::TextWrapped("%s", quirk.c_str());
        }
    }

    // ASI PLUGINS -----------------------------
    if (ScopedCard::Shows("loaded_asi_plugins") && asiPlugins.size() > 0)
    {
        ScopedCard card { "已加载 ASI 插件" };
        for (const auto& name : asiPlugins)
        {
            ImGui::TextWrapped("%s", name.c_str());
        }
    }
}

void MenuCommon::RenderAdvancedSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    const bool upscalerActive = currentFeature != nullptr && !currentFeature->IsFrozen();
    const bool nonDlssActive =
        upscalerActive && currentBackend != Upscaler::DLSS && currentBackend != Upscaler::DLSS_on12;

    // SPOOFING/HOOKING -----------------------------
    if (ScopedCard::Shows("spoofing") && nonDlssActive)
    {
        ScopedCard card { "伪装" };

        if (ImGui::BeginTable("hooking", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox("DXGI 伪装", config->DxgiSpoofing,
                           "为 DXGI 适配器启用 Nvidia GPU 伪装\n"
                           "详细修改可在 INI 中进行",
                           true);

            ImGui::TableNextColumn();
            ConfigCheckbox("Ntdll 挂钩", config->UseNtdllHooks,
                           "Only hook ntdll.dll methods\n"
                           "禁用可切换回内核挂钩",
                           true);

            if (state.api == Vulkan)
            {
                ImGui::TableNextColumn();
                ConfigCheckbox("VLK 伪装", config->VulkanSpoofing,
                               "为 Vulkan 启用 Nvidia GPU 伪装\n"
                               "详细修改可在 INI 中进行",
                               true);

                ImGui::TableNextColumn();
                ConfigCheckbox("VLK 扩展伪装", config->VulkanExtensionSpoofing,
                               "为 Vulkan 启用 Nvidia 扩展伪装\n"
                               "详细修改可在 INI 中进行",
                               true);
            }

            ImGui::TableNextColumn();
            ConfigCheckbox("禁用覆盖层", config->DisableOverlays,
                           "禁用受支持的覆盖层（使用 OptiFG 时自动启用）\n"
                           "包括 Steam Input（可能导致手柄问题）",
                           true);

            ImGui::EndTable();
        }
    }

    // PLUGINS/MISC -----------------------------
    if (ScopedCard::Shows("plugins"))
    {
        ScopedCard card { "启动" };

        if (ImGui::BeginTable("plugins", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox("加载 ASI 插件", config->LoadAsiPlugins,
                           "允许 OptiScaler 从 plugins 文件夹加载 *.asi 文件", true);

            ImGui::TableNextColumn();
            ConfigCheckbox("禁用启动画面消息", config->DisableSplash,
                           "禁用启动画面消息（左下角）", true);

            ImGui::TableNextColumn();
            ConfigCheckbox("检查更新", config->CheckForUpdate,
                           "启用后从 GitHub 检查最新版本\n"
                           "仅适用于稳定/正式版",
                           true);

            ImGui::EndTable();
        }
    }

    // SHADERS, LIMITS AND DRS -----------------------------
    if (ScopedCard::Shows("shaders_limits"))
    {
        ScopedCard card { "着色器" };

        if (ImGui::BeginTable("limits", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ConfigCheckbox("使用预编译着色器", config->UsePrecompiledShaders))
            {
                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }

            // DRS
            ImGui::TableNextColumn();
            ConfigCheckbox("DRS 最小覆盖", config->DrsMinOverrideEnabled,
                           "动态分辨率缩放\n修复忽略官方 DRS 限制的游戏");

            ImGui::TableNextColumn();
            if (upscalerActive)
                ConfigCheckbox("启用扩展限制", config->ExtendedLimits,
                               "扩展画质预设的滑块限制\n\n"
                               "使用此选项会更改分辨率检测逻辑\n"
                               "并可能导致问题和崩溃！");

            ImGui::TableNextColumn();
            ConfigCheckbox("DRS 最大覆盖", config->DrsMaxOverrideEnabled,
                           "动态分辨率缩放\n修复忽略官方 DRS 限制的游戏");

            ImGui::EndTable();
        }
    }

    if (!nonDlssActive)
        return;

    // BARRIERS -----------------------------
    if (ScopedCard::Shows("barriers"))
    {
        ScopedCard card { "资源屏障" };

        if (ImGui::BeginTable("barriers", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            AddResourceBarrier("颜色", &config->ColorResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier("深度", &config->DepthResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier("运动", &config->MVResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier("曝光", &config->ExposureResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier("蒙版", &config->MaskResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier("输出", &config->OutputResourceBarrier);

            ImGui::EndTable();
        }
    }

    // HOTFIXES -----------------------------
    if (ScopedCard::Shows("root_signatures") && state.api == DX12)
    {
        ScopedCard card { "根签名" };

        if (ImGui::BeginTable("rootSignatures", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox("恢复计算根签名", config->RestoreComputeSignature);

            ImGui::TableNextColumn();
            ConfigCheckbox("恢复图形根签名", config->RestoreGraphicSignature);

            ImGui::EndTable();
        }
    }
}

void MenuCommon::RenderLoggingSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // LOGGING -----------------------------
    SectionTitle("日志");

    if (config->LogToConsole.value_or_default() || config->LogToFile.value_or_default() ||
        config->LogToNGX.value_or_default())
        spdlog::default_logger()->set_level((spdlog::level::level_enum) config->LogLevel.value_or_default());
    else
        spdlog::default_logger()->set_level(spdlog::level::off);

    if (bool toFile = config->LogToFile.value_or_default(); ImGui::Checkbox("写入文件", &toFile))
    {
        config->LogToFile = toFile;
        PrepareLogger();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (bool toConsole = config->LogToConsole.value_or_default(); ImGui::Checkbox("输出到控制台", &toConsole))
    {
        config->LogToConsole = toConsole;
        PrepareLogger();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (auto SingleFile = config->LogSingleFile.value_or_default(); ImGui::Checkbox("单文件", &SingleFile))
    {
        config->LogSingleFile = SingleFile;
        PrepareLogger();
    }

    const char* logLevels[] = { "跟踪", "调试", "信息", "警告", "错误" };
    const char* selectedLevel = logLevels[std::clamp(config->LogLevel.value_or_default(), 0, 4)];

    if (ImGui::BeginCombo("日志级别", selectedLevel))
    {
        for (int n = 0; n < 5; n++)
        {
            if (ImGui::Selectable(logLevels[n], (config->LogLevel.value_or_default() == n)))
            {
                config->LogLevel = n;
                spdlog::default_logger()->set_level((spdlog::level::level_enum) config->LogLevel.value_or_default());
            }
        }

        ImGui::EndCombo();
    }
}

void MenuCommon::RenderThemeSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // THEME -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader("菜单主题与颜色"); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool lightTheme = config->LightTheme.value_or_default();

        static constexpr std::pair<const char*, ImVec4> presets[] = {
            { "蓝色", { 0.00f, 0.40f, 0.77f, 1.0f } },   { "青色", { 0.00f, 1.00f, 0.91f, 1.0f } },
            { "灰色", { 0.54f, 0.54f, 0.54f, 1.0f } },   { "黄色", { 1.00f, 0.89f, 0.00f, 1.0f } },
            { "绿色", { 0.25f, 1.00f, 0.00f, 1.0f } },  { "红色", { 1.00f, 0.00f, 0.00f, 1.0f } },
            { "橙色", { 1.00f, 0.52f, 0.00f, 1.0f } }, { "紫色", { 0.576f, 0.00f, 1.00f, 1.0f } },
        };

        // Row of clickable swatches, same presets for the accent and the background
        auto presetSwatches = [&](const char* id, auto& r, auto& g, auto& b)
        {
            const float size = ImGui::GetFrameHeight();

            ImGui::PushID(id);
            for (const auto& [name, color] : presets)
            {
                if (name != presets[0].first)
                    ImGui::SameLine(0.0f, 4.0f);

                if (ImGui::ColorButton(name, toneMapColor(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size)))
                {
                    r = color.x;
                    g = color.y;
                    b = color.z;
                    ApplyThemeStyle();
                }

                ShowTooltip(name);
            }
            ImGui::PopID();
        };

        if (ImGui::Checkbox("浅色主题", &lightTheme))
        {
            config->LightTheme = lightTheme;
            ApplyThemeStyle();
        }

        SectionTitle("强调色");

        presetSwatches("accent", config->MenuAccentColorR, config->MenuAccentColorG, config->MenuAccentColorB);

        float accentColor[3] = { config->MenuAccentColorR.value_or_default(),
                                 config->MenuAccentColorG.value_or_default(),
                                 config->MenuAccentColorB.value_or_default() };

        if (ImGui::ColorEdit3("自定义强调色", accentColor))
        {
            config->MenuAccentColorR = accentColor[0];
            config->MenuAccentColorG = accentColor[1];
            config->MenuAccentColorB = accentColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        if (ImGui::Button("重置强调色"))
        {
            config->MenuAccentColorR.reset();
            config->MenuAccentColorG.reset();
            config->MenuAccentColorB.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        SectionTitle("背景色");

        presetSwatches("bg", config->MenuBGColorR, config->MenuBGColorG, config->MenuBGColorB);

        float bgColor[3] = { config->MenuBGColorR.value_or_default(), config->MenuBGColorG.value_or_default(),
                             config->MenuBGColorB.value_or_default() };

        if (ImGui::ColorEdit3("自定义背景色", bgColor))
        {
            config->MenuBGColorR = bgColor[0];
            config->MenuBGColorG = bgColor[1];
            config->MenuBGColorB = bgColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        auto alpha = config->MenuBGColorA.value_or_default();
        if (ImGui::SliderFloat("背景透明度", &alpha, 0.0f, 1.0f))
        {
            config->MenuBGColorA = alpha;
            ApplyThemeStyle();
        }

        auto rounding = config->MenuRounding.value_or_default();
        if (ImGui::SliderFloat("圆角", &rounding, 0.0f, 16.0f, rounding < 0.5f ? "方形" : "%.0f px"))
        {
            config->MenuRounding = rounding;
            lastMenuScale = 0.0f; // Rescales the style with this rounding
        }

        if (_bgBlurTexture != ImTextureID_Invalid)
        {
            ConfigCheckbox("背景模糊", config->MenuBlur,
                           "模糊菜单背后的游戏画面\n仅当背景透明度低于 1 时可见");

            ImGui::BeginDisabled(!config->MenuBlur.value_or_default());

            auto blurStrength = config->MenuBlurStrength.value_or_default();
            if (ImGui::SliderFloat("模糊强度", &blurStrength, 0.0f, 4.0f, "%.2f"))
                config->MenuBlurStrength = blurStrength;

            ImGui::EndDisabled();
        }

        ImGui::Spacing();

        if (ImGui::Button("重置背景色"))
        {
            config->MenuBGColorR.reset();
            config->MenuBGColorG.reset();
            config->MenuBGColorB.reset();
            config->MenuBGColorA.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();
    }
}

void MenuCommon::RenderFpsOverlaySettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // FPS OVERLAY -----------------------------
    SectionTitle("FPS 覆盖层");
    {
        ScopedID sectionId { "FPS 覆盖层" };

        bool fpsEnabled = config->ShowFps.value_or_default();
        if (ImGui::Checkbox("启用 FPS 覆盖层", &fpsEnabled))
            config->ShowFps = fpsEnabled;

        ImGui::SameLine(0.0f, 6.0f);

        bool fpsHorizontal = config->FpsOverlayHorizontal.value_or_default();
        if (ImGui::Checkbox("水平", &fpsHorizontal))
            config->FpsOverlayHorizontal = fpsHorizontal;

        const char* fpsPosition[] = { "左上",   "右上",     "左下", "右下",
                                      "顶部居中", "底部居中", "自定义" };
        const char* selectedPosition = fpsPosition[config->FpsOverlayPosition.value_or_default()];

        if (ImGui::BeginCombo("覆盖层位置", selectedPosition))
        {
            for (int n = 0; n < std::size(fpsPosition); n++)
            {
                if (ImGui::Selectable(fpsPosition[n], (config->FpsOverlayPosition.value_or_default() == n)))
                    config->FpsOverlayPosition = (FpsOverlayPos) n;
            }

            ImGui::EndCombo();
        }

        if (config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_Custom)
        {
            float posX = config->FpsOverlayPosX.value_or_default();
            if (ImGui::SliderFloat("水平位置", &posX, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayPosX = posX;

            float posY = config->FpsOverlayPosY.value_or_default();
            if (ImGui::SliderFloat("垂直位置", &posY, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayPosY = posY;
        }

        const char* fpsType[] = {
            "仅 FPS", "简洁", "详细", "详细 + 图表", "完整", "完整 + 图表", "低延迟计时",
            "自定义"
        };
        static_assert(std::size(fpsType) == FpsOverlay_COUNT);
        const auto currentType = config->FpsOverlayType.value_or_default();

        if (ImGui::BeginCombo("覆盖层类型", fpsType[currentType]))
        {
            for (int n = 0; n < std::size(fpsType); n++)
            {
                if (ImGui::Selectable(fpsType[n], currentType == n))
                {
                    // Custom starts from the preset's rows the first time
                    if (n == FpsOverlay_Custom && currentType != FpsOverlay_Custom &&
                        !config->FpsOverlayRows.has_value())
                    {
                        SetOverlayRows(OverlayRows(currentType));
                        config->FpsOverlayFpsRowType = OverlayFpsRow(currentType);
                    }

                    config->FpsOverlayType = (FpsOverlay) n;
                }
            }

            ImGui::EndCombo();
        }
        ShowTooltip("行的预设；“自定义”显示在“行”中选择的项目。\n"
                    "覆盖层循环键可在这些预设间切换。");

        // 0 is the menu's scale, then 0.5 to 3.0 in 0.1 steps
        int currentIndex = config->FpsScale.has_value()
                               ? std::clamp((int) std::lround(config->FpsScale.value() * 10.0f) - 4, 1, 26)
                               : 0;
        auto scaleText = currentIndex == 0 ? StrFmt("与菜单相同 (%.1f)", ctx.menuResScale)
                                           : StrFmt("%.1f", 0.4f + currentIndex / 10.0f);

        if (ImGui::SliderInt("缩放", &currentIndex, 0, 26, scaleText.c_str(), ImGuiSliderFlags_ClampOnInput))
        {
            if (currentIndex == 0)
                config->FpsScale.reset();
            else
                config->FpsScale = 0.4f + currentIndex / 10.0f;
        }
        ShowTooltip("FPS 覆盖层大小；除非设为“与菜单相同”，否则独立于菜单缩放");

        auto optionCheckbox = [&](const char* label, CustomOptional<bool>& setting, const char* tip)
        {
            bool value = setting.value_or_default();
            if (ImGui::Checkbox(label, &value))
                setting = value;

            ShowTooltip(tip);
        };

        auto statusText = [](const std::string& text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::PopStyleColor();
        };

        // Which rows are shown and their order, from top to bottom (left to right when horizontal)
        if (ImGui::TreeNode("行"))
        {
            const bool custom = currentType == FpsOverlay_Custom;
            auto rows = OverlayRows(currentType);
            bool rowsChanged = false;

            if (!custom)
            {
                statusText(StrFmt("%s 的行；将覆盖层类型设为“自定义”后才能更改。", fpsType[currentType]));
            }

            ImGui::BeginDisabled(!custom);

            if (ImGui::BeginTable("overlayRows", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("顺序");
                ImGui::TableSetupColumn("行", ImGuiTableColumnFlags_WidthStretch);

                // Drawn as they were at the frame's start, a click changes one row
                const auto shownRows = rows;

                for (size_t i = 0; i < shownRows.size(); i++)
                {
                    const int row = shownRows[i].row;
                    ScopedID rowId { overlayRowKeys[row] };

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::BeginDisabled(i == 0);
                    if (ImGui::ArrowButton("##Up", ImGuiDir_Up))
                    {
                        std::swap(rows[i], rows[i - 1]);
                        rowsChanged = true;
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine(0.0f, 2.0f);

                    ImGui::BeginDisabled(i + 1 == shownRows.size());
                    if (ImGui::ArrowButton("##Down", ImGuiDir_Down))
                    {
                        std::swap(rows[i], rows[i + 1]);
                        rowsChanged = true;
                    }
                    ImGui::EndDisabled();

                    ImGui::TableNextColumn();

                    // A click here means no row moved this frame
                    bool rowOn = shownRows[i].shown;
                    if (ImGui::Checkbox(overlayRowNames[row], &rowOn))
                    {
                        rows[i].shown = rowOn;
                        rowsChanged = true;
                    }

                    ShowTooltip(overlayRowTips[row]);
                }

                ImGui::EndTable();
            }

            if (rowsChanged)
                SetOverlayRows(rows);

            const char* fpsRowTypes[] = { "仅 FPS", "含帧时间", "含平均 FPS" };
            static_assert(std::size(fpsRowTypes) == FpsOverlayFpsRow_COUNT);

            int fpsRowType = OverlayFpsRow(currentType);
            if (ImGui::Combo("FPS 行", &fpsRowType, fpsRowTypes, IM_ARRAYSIZE(fpsRowTypes)))
                config->FpsOverlayFpsRowType = (FpsOverlayFpsRow) fpsRowType;

            ShowTooltip("仅数字，或包含 API、帧时间或平均 FPS、帧生成与超分器");

            ImGui::EndDisabled();

            ImGui::BeginDisabled(custom);
            optionCheckbox("预设中的超分倍率", config->FpsOverlayShowRatio,
                           "从“简洁”起的预设都会显示超分倍率行；“自定义”则在“行”中单独设置");
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 6.0f);

            optionCheckbox("隐藏平均值", config->FpsOverlayHideAverages,
                           "仅显示当前值，不显示 FPS、帧时间与超分器耗时的平均值");

            ImGui::Spacing();

            // -1 is the default spacing
            float rowSpacing = config->FpsOverlayRowSpacing.value_or(-1.0f);
            if (ImGui::SliderFloat("行间距", &rowSpacing, -1.0f, 32.0f,
                                   config->FpsOverlayRowSpacing.has_value() ? "%.0f px" : "默认",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                if (rowSpacing < -0.5f)
                    config->FpsOverlayRowSpacing.reset();
                else
                    config->FpsOverlayRowSpacing = std::round(std::max(rowSpacing, 0.0f));
            }

            if (ImGui::Button(custom ? "重置行与间距" : "重置间距"))
            {
                if (custom)
                {
                    config->FpsOverlayRows.reset();
                    config->FpsOverlayFpsRowType.reset();
                }

                config->FpsOverlayRowSpacing.reset();
            }

            if (custom)
                ShowTooltip("将自定义行恢复为“完整 + 图表”");

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("外观"))
        {
            auto scaleSlider = [&](const char* label, CustomOptional<float>& setting)
            {
                float value = setting.value_or_default();
                if (ImGui::SliderFloat(label, &value, 0.5f, 2.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp))
                    setting = value;
            };

            // Ready made looks, as the text of a settings code: what a preset doesn't list goes back to the default
            SectionTitle("预设");

            struct OverlayPreset
            {
                const char* name;
                const char* tip;
                const char* settings;
            };

            static const OverlayPreset presets[] = {
                { "默认", "OptiScaler 覆盖层：显示所有行与图表，灰色标签带阴影", "" },
                { "经典", "OptiScaler 早期覆盖层：仅白色 FPS，无阴影",
                  "LabelColor=0xFFFFFF;Shadow=0;Rounding=2" },
                { "Afterburner", "仿 MSI Afterburner：Unispace Bold、橙色标签、描边、无背景",
                  "OverlaysUseTheme=false;Type=3;CompactLabels=true;Font=unispace bd.ttf;Alpha=0;Rounding=0;"
                  "LabelColor=0xFF9C00;ValueColor=0xFFFFFF;GraphColor=0xFF9C00;Shadow=2;ShadowStrength=0.5;"
                  "ShadowColor=0x000000;UpdateInterval=500" },
                { "条带", "顶部居中条带：Bahnschrift，深色面板上橙色 FPS 与超分器耗时",
                  "Pos=4;Type=7;Horizontal=true;Rows=fps,-upscale,-frametime,-frametimegraph,upscaler,"
                  "-upscalergraph,-lowlatency;FpsRow=2;UpdateInterval=250;Alpha=0.35;Font=bahnschrift.ttf;"
                  "LetterSpacing=0.5;LabelScale=0.85;ValueScale=1.1;LabelColor=0xFFFFFF;ValueColor=0xFF9A00;"
                  "GraphColor=0x00E5FF;BGColor=0x0A0F14;Rounding=10;HideAverages=true" },
                { "柠檬风味",
                  "用户 Voodoo 的获奖作品：顶部居中条带，Unispace Bold、青柠色数值",
                  "Pos=4;Type=7;Horizontal=true;Rows=upscale,-frametime,-frametimegraph,fps,upscaler,"
                  "-upscalergraph,-lowlatency;FpsRow=1;Alpha=0.12;Font=unispace bd.ttf;ValueColor=0xC9DF21;"
                  "ShadowStrength=0.59;ShadowColor=0x866767;Rounding=7;HideAverages=true" },
            };

            const float presetCount = static_cast<float>(std::size(presets));
            const float buttonWidth =
                (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (presetCount - 1.0f)) /
                presetCount;

            for (size_t i = 0; i < std::size(presets); i++)
            {
                if (i > 0)
                    ImGui::SameLine();

                if (ImGui::Button(presets[i].name, ImVec2(buttonWidth, 0.0f)))
                    config->ApplyOverlaySettings(presets[i].settings);

                ShowTooltip(presets[i].tip);
            }

            SectionTitle("布局");

            optionCheckbox("紧凑标签", config->FpsOverlayCompactLabels,
                           "更短的标签，让覆盖层更窄：UPS、FT、UT");

            float fpsAlpha = config->FpsOverlayAlpha.value_or_default();
            if (ImGui::SliderFloat("背景透明度", &fpsAlpha, 0.0f, 1.0f, "%.2f"))
                config->FpsOverlayAlpha = fpsAlpha;

            float graphHeight = config->FpsOverlayGraphHeight.value_or_default();
            if (ImGui::SliderFloat("图表高度", &graphHeight, 0.5f, 3.0f, "%.1fx", ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayGraphHeight = graphHeight;

            float rounding = config->FpsOverlayRounding.value_or_default();
            if (ImGui::SliderFloat("圆角", &rounding, 0.0f, 16.0f, rounding < 0.5f ? "方形" : "%.0f px",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayRounding = std::round(rounding);
            }

            SectionTitle("文本");

            // Checked once, not every frame: fonts this Windows doesn't have aren't offered
            static const std::vector<bool> overlayFontExists = []()
            {
                std::vector<bool> exists;
                exists.reserve(IM_ARRAYSIZE(overlayFonts));
                for (const auto& [name, file] : overlayFonts)
                    exists.push_back(std::filesystem::exists(OverlayFontPath(file)));
                return exists;
            }();

            auto fontName = config->FpsOverlayFont.value_or("");
            std::string selectedFont = fontName.empty() ? "菜单字体" : fontName;

            for (const auto& [name, file] : overlayFonts)
            {
                if (_stricmp(file, fontName.c_str()) == 0)
                    selectedFont = name;
            }

            if (ImGui::BeginCombo("字体", selectedFont.c_str()))
            {
                if (ImGui::Selectable("菜单字体", fontName.empty()))
                    config->FpsOverlayFont.reset();

                for (size_t i = 0; i < IM_ARRAYSIZE(overlayFonts); ++i)
                {
                    if (!overlayFontExists[i])
                        continue;

                    const auto& [name, file] = overlayFonts[i];
                    if (ImGui::Selectable(name, _stricmp(file, fontName.c_str()) == 0))
                        config->FpsOverlayFont = std::string(file);
                }

                ImGui::EndCombo();
            }
            ShowTooltip("选择 Windows 系统字体，或在下方输入自定义 .ttf/.otf 文件");

            char customFontBuf[260] {};
            std::snprintf(customFontBuf, sizeof(customFontBuf), "%s", fontName.c_str());
            if (ImGui::InputText("自定义字体文件", customFontBuf, sizeof(customFontBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string entered(customFontBuf);
                if (entered.empty())
                    config->FpsOverlayFont.reset();
                else
                    config->FpsOverlayFont = entered;
            }
            ShowTooltip("C:\\Windows\\Fonts 中的文件名或完整路径，回车应用");

            // 7, below the range, is the menu's font size
            float fontSizeValue = config->FpsOverlayFontSize.value_or(7.0f);
            if (ImGui::SliderFloat("字体大小", &fontSizeValue, 7.0f, 48.0f,
                                   config->FpsOverlayFontSize.has_value() ? "%.0f px" : "菜单字体大小",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                if (fontSizeValue < 8.0f)
                    config->FpsOverlayFontSize.reset();
                else
                    config->FpsOverlayFontSize = std::round(fontSizeValue);
            }
            ShowTooltip("在覆盖层缩放之前，最左侧为菜单字体大小");

            float letterSpacing = config->FpsOverlayLetterSpacing.value_or_default();
            if (ImGui::SliderFloat("字间距", &letterSpacing, -2.0f, 8.0f, "%.1f px",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayLetterSpacing = letterSpacing;
            }
            ShowTooltip("字符间额外的水平字距（像素）");

            scaleSlider("标签大小", config->FpsOverlayLabelScale);
            ShowTooltip("名称如 FPS: 与 Avg:");

            scaleSlider("数值大小", config->FpsOverlayValueScale);
            ShowTooltip("数字部分");

            float updateInterval = config->FpsOverlayUpdateInterval.value_or_default();
            if (ImGui::SliderFloat("更新间隔", &updateInterval, 0.0f, 1000.0f,
                                   updateInterval < 1.0f ? "每帧" : "%.0f ms", ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayUpdateInterval = std::round(updateInterval / 10.0f) * 10.0f;
            }
            ShowTooltip("FPS 与耗时的更新频率，其间取平均。数值越高越易读。");

            const char* shadowTypes[] = { "关", "阴影", "描边" };
            int shadowType = config->FpsOverlayShadowType.value_or_default();
            if (ImGui::Combo("文字阴影", &shadowType, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
                config->FpsOverlayShadowType = (FpsOverlayShadow) shadowType;

            ImGui::BeginDisabled(shadowType == FpsOverlayShadow_Off);
            float shadowStrength = config->FpsOverlayShadowStrength.value_or_default();
            if (ImGui::SliderFloat("阴影强度", &shadowStrength, 0.0f, 1.0f, "%.2f",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayShadowStrength = shadowStrength;
            }
            ShowTooltip("阴影的粗细与深浅，描边的粗细");
            ImGui::EndDisabled();

            SectionTitle("颜色");

            // Side by side, an item that doesn't fit goes to the next line
            const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            bool firstColorItem = true;

            auto colorRowItem = [&](const char* label)
            {
                const auto& style = ImGui::GetStyle();
                const float gap = style.ItemSpacing.x * 2.0f;
                const float width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;

                if (!firstColorItem && ImGui::GetItemRectMax().x + gap + width <= rowRight)
                    ImGui::SameLine(0.0f, gap);

                firstColorItem = false;
            };

            auto colorEdit = [&](const char* label, CustomOptional<uint32_t>& setting)
            {
                colorRowItem(label);

                auto color = RgbToColor(setting.value_or_default());
                float rgb[3] = { color.x, color.y, color.z };

                if (ImGui::ColorEdit3(label, rgb, ImGuiColorEditFlags_NoInputs))
                    setting = ColorToRgb(rgb);
            };

            bool useTheme = config->OverlaysUseTheme.value_or_default();
            if (ImGui::Checkbox("使用主题", &useTheme))
                config->OverlaysUseTheme = useTheme;

            ShowTooltip("菜单主题用于标签、数值、图表与背景的颜色");

            ImGui::BeginDisabled(useTheme);
            colorEdit("标签", config->FpsOverlayLabelColor);
            colorEdit("数值", config->FpsOverlayValueColor);
            colorEdit("图表", config->FpsOverlayGraphColor);
            colorEdit("背景", config->FpsOverlayBGColor);
            ImGui::EndDisabled();

            ImGui::BeginDisabled(shadowType == FpsOverlayShadow_Off);
            colorEdit("阴影", config->FpsOverlayShadowColor);
            ImGui::EndDisabled();

            ImGui::Spacing();

            if (ImGui::Button("重置外观"))
            {
                config->FpsOverlayCompactLabels.reset();
                config->FpsOverlayFont.reset();
                config->FpsOverlayFontSize.reset();
                config->FpsOverlayLetterSpacing.reset();
                config->FpsOverlayUpdateInterval.reset();
                config->FpsOverlayLabelColor.reset();
                config->FpsOverlayValueColor.reset();
                config->FpsOverlayLabelScale.reset();
                config->FpsOverlayValueScale.reset();
                config->FpsOverlayGraphColor.reset();
                config->FpsOverlayBGColor.reset();
                config->FpsOverlayShadowType.reset();
                config->FpsOverlayShadowStrength.reset();
                config->FpsOverlayShadowColor.reset();
                config->FpsOverlayGraphHeight.reset();
                config->FpsOverlayRounding.reset();
                config->FpsOverlayAlpha.reset();
            }

            ImGui::TreePop();
        }

        // The overlay's settings as a code, kept outside the ini across updates and shared with others
        if (ImGui::TreeNode("分享设置"))
        {
            static std::string settingsCodeStatus;

            if (ImGui::Button("复制代码"))
            {
                ImGui::SetClipboardText(config->ExportOverlaySettings().c_str());
                settingsCodeStatus = "已复制到剪贴板";
            }

            ShowTooltip("将覆盖层设置复制为代码以便保存或分享");

            ImGui::SameLine();

            if (ImGui::Button("粘贴代码"))
            {
                const char* clipboard = ImGui::GetClipboardText();

                if (clipboard != nullptr && config->ImportOverlaySettings(clipboard))
                    settingsCodeStatus = "已应用，保存设置以保留";
                else
                    settingsCodeStatus = "剪贴板中没有 FPS 覆盖层代码";
            }

            ShowTooltip("应用复制的代码。未列出的设置将恢复默认值。");

            if (!settingsCodeStatus.empty())
                statusText(settingsCodeStatus);

            ImGui::TreePop();
        }
    }
}

void MenuCommon::RenderUpscalerInputsSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // UPSCALER INPUTS -----------------------------
    SectionTitle("超分器输入");
    {
        ScopedID sectionId { "超分器输入" };

        // Values when the menu was first drawn, enabling and pattern matching apply on next launch
        static const bool startFsr2 = config->EnableFsr2Inputs.value_or_default();
        static const bool startFsr3 = config->EnableFsr3Inputs.value_or_default();
        static const bool startFfx = config->EnableFfxInputs.value_or_default();
        static const bool startFsr2Pattern = config->Fsr2Pattern.value_or_default();
        static const bool startFsr3Pattern = config->Fsr3Pattern.value_or_default();

        if (ImGui::BeginTable("upscalerInputs", 4, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn("输入", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("启用", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("使用", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("模式", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            ImGui::TableNextRow();
            for (int i = 0; i < 4; i++)
            {
                ImGui::TableSetColumnIndex(i);
                ImGui::TextDisabled("%s", ImGui::TableGetColumnName(i));

                if (i == 1)
                    ShowTooltip("挂钩此 API 的输入，使用前必须先挂钩");
                else if (i == 2)
                    ShowTooltip("从此 API 获取超分器输入");
                else if (i == 3)
                    ShowTooltip("通过模式匹配查找输入");
            }

            auto row = [&](const char* name, auto& enable, auto& use, auto pattern)
            {
                ScopedID rowId { name };

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name);

                ImGui::TableNextColumn();
                ConfigCheckbox("##enable", enable, "挂钩此 API 的输入", true);

                ImGui::BeginDisabled(!enable.value_or_default());

                ImGui::TableNextColumn();
                ConfigCheckbox("##use", use, "从此 API 获取超分器输入");

                ImGui::TableNextColumn();
                if constexpr (!std::is_null_pointer_v<decltype(pattern)>)
                    ConfigCheckbox("##pattern", *pattern, "通过模式匹配查找输入", true);

                ImGui::EndDisabled();
            };

            row("FSR2", config->EnableFsr2Inputs, config->UseFsr2Inputs, &config->Fsr2Pattern);
            row("FSR3", config->EnableFsr3Inputs, config->UseFsr3Inputs, &config->Fsr3Pattern);
            row("FFX", config->EnableFfxInputs, config->UseFfxInputs, nullptr);

            ImGui::EndTable();
        }

        const bool restartNeeded = startFsr2 != config->EnableFsr2Inputs.value_or_default() ||
                                   startFsr3 != config->EnableFsr3Inputs.value_or_default() ||
                                   startFfx != config->EnableFfxInputs.value_or_default() ||
                                   startFsr2Pattern != config->Fsr2Pattern.value_or_default() ||
                                   startFsr3Pattern != config->Fsr3Pattern.value_or_default();

        if (restartNeeded)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
            ImGui::TextWrapped("保存设置并重启游戏，以应用启用状态与模式更改");
            ImGui::PopStyleColor();
        }
    }
}

void MenuCommon::RenderVsyncSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // DX11 & DX12 -----------------------------
    if (state.swapchainApi != Vulkan)
    {
        // V-SYNC -----------------------------
        SectionTitle("V-Sync 设置");
        {
            ScopedID sectionId { "V-Sync 设置" };

            auto OverrideVsync = config->OverrideVsync.value_or_default();
            {
                if (ImGui::Checkbox("覆盖 V-Sync", &OverrideVsync))
                {
                    config->OverrideVsync = OverrideVsync;
                    LOG_DEBUG("Enabled OverrideVsync: {}", OverrideVsync);
                }
                ShowTooltip("强制覆盖游戏的 V-Sync 设置。\n\n"
                            "例如当 XeFG 被锁定在 V-Sync 帧率上限时很有用\n"
                            "即使关闭 V-Sync 仍受帧率限制（例如《仁王 3》）\n\n"
                            "更改后，请保存设置并重启。");
            }

            auto forceVsyncOn = config->ForceVsync.has_value() && config->ForceVsync.value();
            auto forceVsyncOff = config->ForceVsync.has_value() && !config->ForceVsync.value();
            bool vsyncChanged = false;

            if (ImGui::Checkbox("强制开启 V-Sync", &forceVsyncOn))
            {
                if (forceVsyncOn)
                {
                    config->ForceVsync = true;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Checkbox("强制关闭 V-Sync", &forceVsyncOff))
            {
                if (forceVsyncOff)
                {
                    config->ForceVsync = false;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!forceVsyncOn);

            ImGui::PushItemWidth(50.0f * menuResScale);

            auto vsyncBuf = StrFmt("%d", config->VsyncInterval.value_or_default());
            if (ImGui::BeginCombo("同步间隔", vsyncBuf.c_str()))
            {
                if (ImGui::Selectable("0", config->VsyncInterval.value_or_default() == 0))
                {
                    config->VsyncInterval = 0;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("1", config->VsyncInterval.value_or_default() == 1))
                {
                    config->VsyncInterval = 1;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("2", config->VsyncInterval.value_or_default() == 2))
                {
                    config->VsyncInterval = 2;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("3", config->VsyncInterval.value_or_default() == 3))
                {
                    config->VsyncInterval = 3;
                    vsyncChanged = true;
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ShowTooltip("控制 DXGI Present 同步间隔，它决定了\n"
                        "交换链如何等待垂直刷新。\n\n"
                        "0 = 立即呈现，不等待 VSync。\n"
                        "1 = 每次刷新同步，正常 VSync。\n"
                        "2+ = 每 N 次刷新呈现一次，降低有效帧率。\n\n"
                        "较高的值可以减少撕裂，但可能增加延迟并限制 FPS。\n"
                        "大多数游戏建议使用 0 获得最低延迟，或使用 1 获得正常 VSync。");

            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button("重置##10"))
            {
                config->ForceVsync.reset();
                vsyncChanged = true;
            }

            ShowTooltip("强制 V-Sync 开关及同步间隔选项");

            if (vsyncChanged && state.activeFgOutput == FGOutput::XeFG && state.currentFG != nullptr)
            {
                // To prevent XeLL issues
                LOG_DEBUG("检测到 V-Sync 变化，强制重置 XeFG");
                state.WAR_xefgRequestFGToggle = true;
            }
        }
    }
    else
    {
        SectionTitle("V-Sync 设置");
        {
            ScopedID sectionId { "V-Sync 设置" };

            const auto& forced = config->VulkanPresentMode;
            const char* preview = forced.has_value() ? VulkanHooks::PresentModeName(forced.value()) : "游戏默认";
            bool changed = false;

            if (ImGui::BeginCombo("呈现模式", preview))
            {
                if (ImGui::Selectable("游戏默认", !forced.has_value()))
                {
                    config->VulkanPresentMode.reset();
                    changed = true;
                }

                for (uint32_t i = 0; i < std::size(VulkanHooks::PresentModes); i++)
                {
                    const auto& entry = VulkanHooks::PresentModes[i];

                    if ((state.vkPresentModeMask & (1u << i)) == 0)
                        continue;

                    if (ImGui::Selectable(entry.name, forced.has_value() && forced.value() == entry.mode))
                    {
                        config->VulkanPresentMode = (int) entry.mode;
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            ShowTooltip("覆盖游戏的 Vulkan 呈现模式，游戏会重建交换链以应用。\n\n"
                        "Immediate：无 V-Sync，会撕裂。\n"
                        "Mailbox：无撕裂，最新帧会替换队列中的帧。\n"
                        "FIFO：V-Sync。\n"
                        "FIFO Relaxed：V-Sync，低于刷新率时撕裂。\n"
                        "FIFO Latest Ready：V-Sync，每次刷新显示最新的就绪帧。\n\n"
                        "Mailbox 与 FIFO Latest Ready 在高于刷新率时可能丢弃生成帧。\n"
                        "仅列出显示器支持的模式。");

            if (state.vkGamePresentMode >= 0)
                ImGui::Text("游戏呈现模式：%s", VulkanHooks::PresentModeName(state.vkGamePresentMode));

            if (changed)
                state.vkPresentModeChanged = true;
        }
    }
}

void MenuCommon::RenderApiAndTextureSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // DX11 & DX12 -----------------------------
    if (state.swapchainApi != Vulkan)
    {
        // MIPMAP BIAS & Anisotropy -----------------------------
        if (ScopedCard::Shows("mipmap"))
        {
            ScopedCard card { "Mipmap 偏移" };
            ScopedID sectionId { "Mipmap 偏移" };
            if (config->MipmapBiasOverride.has_value() && _mipBias == 0.0f)
                _mipBias = config->MipmapBiasOverride.value();

            ImGui::SliderFloat("Mipmap 偏移##2", &_mipBias, -15.0f, 15.0f, "%.6f");
            ShowTooltip("可帮助解决问题游戏中纹理模糊的问题\n"
                        "负值使纹理更锐利\n"
                        "正值使纹理更模糊\n\n"
                        "对性能影响较小");

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                ImGui::BeginDisabled(config->MipmapBiasScaleOverride.has_value() &&
                                     config->MipmapBiasScaleOverride.value());
                {
                    bool mbFixed = config->MipmapBiasFixedOverride.value_or_default();
                    if (ImGui::Checkbox("固定值覆盖 Mipmap", &mbFixed))
                    {
                        config->MipmapBiasScaleOverride.reset();
                        config->MipmapBiasFixedOverride = mbFixed;
                    }

                    ShowTooltip("对所有纹理应用相同的覆盖值");
                }
                ImGui::EndDisabled();

                ImGui::SameLine(0.0f, 6.0f);

                ImGui::BeginDisabled(config->MipmapBiasFixedOverride.has_value() &&
                                     config->MipmapBiasFixedOverride.value());
                {
                    bool mbScale = config->MipmapBiasScaleOverride.value_or_default();
                    if (ImGui::Checkbox("缩放覆盖 Mipmap", &mbScale))
                    {
                        config->MipmapBiasFixedOverride.reset();
                        config->MipmapBiasScaleOverride = mbScale;
                    }

                    ShowTooltip("将覆盖值作为缩放倍率应用\n"
                                "使用缩放模式时，请使用正值\n"
                                "覆盖值以增加锐化！");
                }
                ImGui::EndDisabled();

                bool mbAll = config->MipmapBiasOverrideAll.value_or_default();
                if (ImGui::Checkbox("覆盖所有纹理", &mbAll))
                    config->MipmapBiasOverrideAll = mbAll;

                ShowTooltip("覆盖所有纹理的 mipmap 值\n"
                            "通常 OptiScaler 只覆盖\n"
                            "低于零的 mipmap 值！");
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(config->MipmapBiasOverride.has_value() &&
                                 config->MipmapBiasOverride.value() == _mipBias);
            {
                if (ImGui::Button("设置"))
                {
                    config->MipmapBiasOverride = _mipBias;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 6.0f);

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                if (ImGui::Button("重置"))
                {
                    config->MipmapBiasOverride.reset();
                    _mipBias = 0.0f;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            if (currentFeature != nullptr && !currentFeature->IsFrozen())
            {
                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("计算 Mipmap 偏移"))
                    _showMipmapCalcWindow = true;
            }

            if (config->MipmapBiasOverride.has_value())
            {
                if (config->MipmapBiasFixedOverride.value_or_default())
                {
                    ImGui::Text("当前：%.3f / %.3f，目标：%.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else if (config->MipmapBiasScaleOverride.value_or_default())
                {
                    ImGui::Text("当前：%.3f / %.3f，目标：基准 × %.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else
                {
                    ImGui::Text("当前：%.3f / %.3f，目标：基准 + %.3f", state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
            }
            else
            {
                ImGui::Text("当前：%.3f / %.3f", state.lastMipBias, state.lastMipBiasMax);
            }

            ImGui::Text("将在分辨率/预设更改后生效！！！");
        }

        if (ScopedCard::Shows("anisotropy"))
        {
            ScopedCard card { "各向异性过滤" };
            ScopedID sectionId { "各向异性过滤" };
            ImGui::PushItemWidth(65.0f * menuResScale);

            auto selectedAF =
                config->AnisotropyOverride.has_value() ? std::to_string(config->AnisotropyOverride.value()) : "自动";
            if (ImGui::BeginCombo("强制各向异性过滤", selectedAF.c_str()))
            {
                if (ImGui::Selectable("自动", !config->AnisotropyOverride.has_value()))
                    config->AnisotropyOverride.reset();

                if (ImGui::Selectable("1", config->AnisotropyOverride.value_or(0) == 1))
                    config->AnisotropyOverride = 1;

                if (ImGui::Selectable("2", config->AnisotropyOverride.value_or(0) == 2))
                    config->AnisotropyOverride = 2;

                if (ImGui::Selectable("4", config->AnisotropyOverride.value_or(0) == 4))
                    config->AnisotropyOverride = 4;

                if (ImGui::Selectable("8", config->AnisotropyOverride.value_or(0) == 8))
                    config->AnisotropyOverride = 8;

                if (ImGui::Selectable("16", config->AnisotropyOverride.value_or(0) == 16))
                    config->AnisotropyOverride = 16;

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            bool afComp = config->AnisotropyModifyComp.value_or_default();
            if (ImGui::Checkbox("修改比较", &afComp))
                config->AnisotropyModifyComp = afComp;

            ShowTooltip("更新比较过滤器");

            ImGui::SameLine(0.0f, 6.0f);

            bool afMinMax = config->AnisotropyModifyMinMax.value_or_default();
            if (ImGui::Checkbox("修改最小/最大", &afMinMax))
                config->AnisotropyModifyMinMax = afMinMax;

            ShowTooltip("更新最小/最大过滤器");

            bool afSkipPoint = config->AnisotropySkipPointFilter.value_or_default();
            if (ImGui::Checkbox("跳过点过滤", &afSkipPoint))
                config->AnisotropySkipPointFilter = afSkipPoint;

            ShowTooltip("跳过点过滤器的更新");

            ImGui::Text("可能在分辨率/预设更改后生效！！！");
        }
    }
}

void MenuCommon::RenderKeybindSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    SectionTitle("按键绑定");
    {
        ScopedID sectionId { "按键绑定" };

        ImGui::Text("当前不支持组合键！");
        ImGui::Text("按 Esc 取消，按退格键解除绑定");
        ImGui::Spacing();

        static auto menu = Keybind("菜单", 10);
        static auto fpsOverlay = Keybind("FPS 覆盖层", 11);
        static auto fpsOverlayCycle = Keybind("FPS 覆盖层循环切换", 12);
        static auto fgEnable = Keybind("帧生成", 13);

        menu.Render(config->ShortcutKey);
        fpsOverlay.Render(config->FpsShortcutKey);
        fpsOverlayCycle.Render(config->FpsCycleShortcutKey);
        fgEnable.Render(config->FGShortcutKey);
    }
}

void MenuCommon::RenderMenuScaleSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;

    SectionTitle("菜单");

    ImGui::PushItemWidth(100.0f * menuResScale);

    auto autoText = config->MenuScale.has_value() ? "自动" : StrFmt("自动（%3.1f）", menuResScale);
    // clang-format off
    const char* uiScales[] = { autoText.c_str(), "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1",
                               "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
    // clang-format on

    const char* selectedScaleName = uiScales[_selectedScale];

    if (ImGui::BeginCombo("菜单缩放", selectedScaleName))
    {
        for (int n = 0; n < std::size(uiScales); n++)
        {
            if (ImGui::Selectable(uiScales[n], (_selectedScale == n)))
            {
                _selectedScale = n;

                if (n == 0)
                    config->MenuScale.reset();
                else
                    config->MenuScale = 0.4f + (float) n / 10.0f;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
}

static const char* ApiName(API api)
{
    if (api == DX11)
        return "D3D11";

    if (api == DX12)
        return "D3D12";

    if (api == Vulkan)
        return "Vulkan";

    return "未知";
}

// Set by the DLSS inputs, before an upscaler is created the swapchain shows it
static API GameApi(const State& state)
{
    if (state.api != NotSelected)
        return state.api;

    if (state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12)
        return DX11;

    return state.swapchainApi;
}

// Filled dot for on / found, hollow dot for off / missing. Goes into the next table cell
static void StatusFlag(const char* label, bool on, bool warnWhenOn = false)
{
    ImGui::TableNextColumn();

    const float lineHeight = ImGui::GetTextLineHeight();
    const float radius = lineHeight * 0.28f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 center(pos.x + lineHeight * 0.5f, pos.y + lineHeight * 0.5f);
    auto drawList = ImGui::GetWindowDrawList();

    if (on)
        drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(warnWhenOn ? StatusWarn() : StatusOk()));
    else
        drawList->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_TextDisabled), 0, 1.5f);

    ImGui::Dummy(ImVec2(lineHeight, lineHeight));
    ImGui::SameLine(0.0f, 0.0f);

    if (on)
        ImGui::TextUnformatted(label);
    else
        ImGui::TextDisabled("%s", label);
}

// Which libraries and GPU features were found. Used by the Status tab and when no upscaler is available
void MenuCommon::RenderDetectedInfo(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& primaryGpu = *ctx.primaryGpu;

    if (!ScopedCard::Shows("status_detected"))
        return;

    ScopedCard card { "检测结果" };

    if (ImGui::BeginTable("statusDetected", 4, ImGuiTableFlags_SizingStretchProp))
    {
        StatusFlag("nvngx.dll", state.nvngxExists);
        StatusFlag("nvngx_dlss", state.NVNGX_DLSS_Path.has_value());
        StatusFlag("nvngx_dlssd", state.NVNGX_DLSSD_Path.has_value());
        StatusFlag("nvngx 替换", state.nvngxReplacement.has_value());
        StatusFlag("libxess", state.libxessExists || XeSSProxy::Module() != nullptr);
        StatusFlag("FSR 挂钩", state.fsrHooks);
        StatusFlag("FFX 3.1", FfxApiProxy::Dx12Module() != nullptr);
        StatusFlag("FFX SR", FfxApiProxy::Dx12Module_SR() != nullptr);
        StatusFlag("FFX 帧生成", FfxApiProxy::Dx12Module_FG() != nullptr);
        StatusFlag("FFX Vk", FfxApiProxy::VkModule() != nullptr);
        StatusFlag("FFX Vk SR", FfxApiProxy::VkModule_SR() != nullptr);
        StatusFlag("FFX Vk FG", FfxApiProxy::VkModule_FG() != nullptr);
        StatusFlag("OptiPatcher", state.isOptiPatcherSucceed);
        StatusFlag("Linux", state.isRunningOnLinux);
        StatusFlag("支持 DLSS 的 GPU", primaryGpu.dlssCapable);
        StatusFlag("支持 FSR4 的 GPU", primaryGpu.fsr4Support != FSR4Support::None, IsFsr4Int8(primaryGpu.fsr4Support));

        ImGui::EndTable();
    }
}

// Same dense look as the Status tab
void MenuCommon::RenderDetectedCard(RenderMenuContext& ctx)
{
    const auto& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style.CellPadding.x, 1.0f * ctx.menuResScale));

    RenderDetectedInfo(ctx);

    ImGui::PopStyleVar(2);
}

void MenuCommon::RenderStatusInfo(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    const bool active = currentFeature != nullptr && !currentFeature->IsFrozen();

    // Dense on purpose, the whole page is meant to fit in a single screenshot
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 1.0f * menuResScale));

    // Label and value, two pairs per row
    auto pair = [&](const char* label, const std::string& value, bool dim = false)
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", label);

        ImGui::TableNextColumn();
        if (dim)
            ImGui::TextDisabled("%s", value.c_str());
        else
            ImGui::TextUnformatted(value.c_str());
    };

    auto flag = [](const char* label, bool on, bool warnWhenOn = false) { StatusFlag(label, on, warnWhenOn); };

    auto setupPairs = []()
    {
        ImGui::TableSetupColumn("##l1", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("##v1", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("##l2", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("##v2", ImGuiTableColumnFlags_WidthStretch, 1.1f);
    };

    // SYSTEM -----------------------------
    if (ScopedCard::Shows("status_system"))
    {
        ScopedCard card { "系统" };

        // Long values get the whole row
        if (ImGui::BeginTable("statusSystem", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("##label");
            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

            auto row = [&](const char* label, const std::string& value)
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            };

            row("GPU", primaryGpu.name);
            row("游戏", state.gameExe + (state.gameName.empty() ? "" : " (" + state.gameName + ")"));

            if (state.detectedQuirks.size() > 0)
            {
                std::string quirks;
                for (const auto& quirk : state.detectedQuirks)
                    quirks += quirks.empty() ? quirk : ", " + quirk;

                ImGui::TableNextColumn();
                ImGui::TextDisabled("兼容性修正");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", quirks.c_str());
            }

            const auto asiPlugins = ListedAsiPlugins(state);
            if (asiPlugins.size() > 0)
            {
                std::string asi;
                for (const auto& name : asiPlugins)
                    asi += asi.empty() ? name : ", " + name;
                ImGui::TableNextColumn();
                ImGui::TextDisabled("ASI");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", asi.c_str());
            }

            ImGui::EndTable();
        }

        std::string ratioOverride = "关";
        if (config->UpscaleRatioOverrideEnabled.value_or_default())
            ratioOverride = StrFmt("全部 %.2f", config->UpscaleRatioOverrideValue.value_or_default());
        else if (config->QualityRatioOverrideEnabled.value_or_default())
            ratioOverride = "按预设";

        if (ImGui::BeginTable("statusRuntime", 4, ImGuiTableFlags_SizingStretchProp))
        {
            setupPairs();

            pair("API", StrFmt("%s%s", ApiName(GameApi(state)),
                               IdentifyGpu::gameUsesDxvk()          ? " (DXVK)"
                               : IdentifyGpu::gameUsesVkd3dProton() ? " (VKD3D)"
                                                                    : ""));
            pair("交换链", ApiName(state.swapchainApi));
            pair("输入", ApiUpscalerInputName(state.currentInputApiName));

            if (active)
            {
                pair("超分器", NameWithVersion(currentFeature) +
                                     ((currentFeature->IsWithDx12() && state.api != DX12) ? " 搭配 Dx12" : ""));
                pair("渲染", StrFmt("%dx%d", currentFeature->RenderWidth(), currentFeature->RenderHeight()));
                pair("输出", StrFmt("%dx%d (%.2fx)", currentFeature->TargetWidth(), currentFeature->TargetHeight(),
                                      (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth()));
                pair("显示", StrFmt("%dx%d (%.2fx)", currentFeature->DisplayWidth(), currentFeature->DisplayHeight(),
                                       (float) currentFeature->DisplayWidth() / (float) currentFeature->RenderWidth()));
                pair("帧数", std::to_string(currentFeature->FrameCount()));
            }
            else
            {
                pair("超分器", "无", true);
                pair("渲染", "-", true);
                pair("输出", "-", true);
                pair("显示", "-", true);
                pair("帧数", "-", true);
            }

            auto fgInput = state.activeFgInput;
            if (fgInput != FGInput::NoFG)
                pair("帧生成输入", EnumToCode(fgInput));
            else
                pair("帧生成输入", "关", true);

            auto fgOutput = state.activeFgOutput;
            if (fgOutput != FGOutput::NoFG)
                pair("帧生成输出", EnumToCode(fgOutput));
            else
                pair("帧生成输出", "关", true);

            pair("倍率覆盖", ratioOverride, ratioOverride == "关");
            pair("Mipmap 偏移",
                 config->MipmapBiasOverride.has_value() ? StrFmt("%.3f", config->MipmapBiasOverride.value()) : "关",
                 !config->MipmapBiasOverride.has_value());
            pair("各向异性",
                 config->AnisotropyOverride.has_value() ? std::to_string(config->AnisotropyOverride.value()) : "自动",
                 !config->AnisotropyOverride.has_value());

            ImGui::EndTable();
        }
    }

    // DETECTED -----------------------------
    RenderDetectedInfo(ctx);

    // HOOKS AND CONFIG -----------------------------
    if (ScopedCard::Shows("status_hooks"))
    {
        ScopedCard card { "挂钩" };

        if (ImGui::BeginTable("statusConfig", 4, ImGuiTableFlags_SizingStretchSame))
        {
            flag("DXGI 伪装", config->DxgiSpoofing.value_or_default());
            flag("VLK 伪装", config->VulkanSpoofing.value_or_default());
            flag("VLK 扩展伪装", config->VulkanExtensionSpoofing.value_or_default());
            flag("Ntdll 挂钩", config->UseNtdllHooks.value_or_default());
            flag("禁用覆盖层", config->DisableOverlays.value_or_default(), true);
            flag("ASI 插件", config->LoadAsiPlugins.value_or_default());
            flag("预编译着色器", config->UsePrecompiledShaders.value_or_default());
            flag("扩展限制", config->ExtendedLimits.value_or_default(), true);
            flag("DRS 最小覆盖", config->DrsMinOverrideEnabled.value_or_default());
            flag("DRS 最大覆盖", config->DrsMaxOverrideEnabled.value_or_default());
            flag("计算根签名", config->RestoreComputeSignature.value_or_default());
            flag("图形根签名", config->RestoreGraphicSignature.value_or_default());

            ImGui::EndTable();
        }

        // Only shown when something was overridden
        std::string barriers;
        auto addBarrier = [&](const char* name, bool set)
        {
            if (!set)
                return;

            barriers += barriers.empty() ? name : std::string(", ") + name;
        };
        addBarrier("颜色", config->ColorResourceBarrier.has_value());
        addBarrier("深度", config->DepthResourceBarrier.has_value());
        addBarrier("运动", config->MVResourceBarrier.has_value());
        addBarrier("曝光", config->ExposureResourceBarrier.has_value());
        addBarrier("蒙版", config->MaskResourceBarrier.has_value());
        addBarrier("输出", config->OutputResourceBarrier.has_value());

        if (!barriers.empty())
        {
            ImGui::TextDisabled("资源屏障已覆盖");
            ImGui::SameLine();
            ImGui::TextUnformatted(barriers.c_str());
        }
    }

    // INIT FLAGS -----------------------------
    if (ScopedCard::Shows("status_init") && active)
    {
        ScopedCard card { "初始化标志" };

        if (ImGui::BeginTable("statusInit", 4, ImGuiTableFlags_SizingStretchSame))
        {
            flag("自动曝光", currentFeature->AutoExposure());
            flag("深度反转", currentFeature->DepthInverted());
            flag("HDR", currentFeature->IsHdr());
            flag("显示分辨率运动矢量", !currentFeature->LowResMV());
            flag("抖动消除", currentFeature->JitteredMV());
            flag("动态分辨率", currentFeature->DynamicResolution());
            flag("输出缩放", config->OutputScalingEnabled.value_or_default());
            flag("锐化覆盖", config->OverrideSharpness.value_or_default());
            flag("V-Sync 覆盖", config->OverrideVsync.value_or_default());

            ImGui::EndTable();
        }
    }

    ImGui::PopStyleVar(2);
}

static bool IsUpscalerActive(IFeature* feature) { return feature != nullptr && !feature->IsFrozen(); }

static float FooterPlotHeight() { return ImGui::GetFrameHeight() * 1.0f; }

// Everything RenderMainMenuFooter draws, every item is followed by the item spacing
static float FooterHeight()
{
    const auto& style = ImGui::GetStyle();

    return 1.0f + style.ItemSpacing.y * 5.0f + ImGui::GetTextLineHeight() + FooterPlotHeight() +
           ImGui::GetFrameHeight();
}

// CUSTOM TAB -----------------------------
struct MenuCommon::MenuBox
{
    const char* id;   // Saved to the config
    const char* tab;  // Label of the tab it belongs to
    const char* name; // Shown in the Custom tab picker
    void (*draw)(RenderMenuContext&);
    // Function that draws several boxes, ScopedCard::Shows(id) selects the one to draw
    void (*source)(RenderMenuContext&) = nullptr;
    bool pinnable = true;
};

// Every box of every tab in the order they are drawn
const MenuCommon::MenuBox* MenuCommon::GetMenuBoxes(size_t& count)
{
    // clang-format off
    static const MenuBox boxes[] = {
        { "upscaler", "超分", "超分器",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderActiveUpscalerSettings); } },
        { "sharpness", "超分", "锐化",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderActiveImageSettings); } },
        { "upscale_ratio", "超分", "超分倍率覆盖",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderUpscaleRatioSettings); } },
        { "output_scaling", "超分", "输出缩放",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderOutputScalingSettings); } },
        { "shader_times", "超分", "各着色器 GPU 耗时",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderShaderTimes); } },
        { "magnifier", "超分", "放大镜",
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderMagnifierSettings); } },

        { "fg_selection", "帧生成", "帧生成", [](RenderMenuContext& c) { RenderCard(c, RenderFrameGenerationSelection); } },
        { "fg_fsrfg", "帧生成", "FSR FG 设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_xefg", "帧生成", "XeFG 设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_dlssg", "帧生成", "DLSSG 设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_flipmeter", "帧生成", "翻转节流设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_reprojection", "帧生成", "重投影设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_optifg", "帧生成", "OptiFG 设置", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_nvngx", "帧生成", "Nvngx FG 替换", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_fsrfg_inputs", "帧生成", "FSR-FG 输入", nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_sl_inputs", "帧生成", "Streamline FG 输入", nullptr, RenderFrameGenerationRuntimeSettings },

        { "framerate", "延迟", "帧率", [](RenderMenuContext& c) { RenderCard(c, RenderFramerateSettings); } },
        { "low_latency", "延迟", "低延迟", [](RenderMenuContext& c) { RenderCard(c, RenderLowLatencySettings); } },
        { "vsync", "延迟", "V-Sync", [](RenderMenuContext& c) { RenderCard(c, RenderVsyncSettings); } },

        { "mipmap", "纹理", "Mipmap 偏移", nullptr, RenderApiAndTextureSettings },
        { "anisotropy", "纹理", "各向异性过滤", nullptr, RenderApiAndTextureSettings },

        { "init_flags", "兼容性", "初始化标志", [](RenderMenuContext& c) { RenderCard(c, RenderInitFlagsSettings); } },
        { "fsr_common", "兼容性", "FSR 通用设置", [](RenderMenuContext& c) { RenderCard(c, RenderFsrCommonSettings); } },
        { "upscaler_inputs", "兼容性", "超分器输入", [](RenderMenuContext& c) { RenderCard(c, RenderUpscalerInputsSettings); } },
        { "spoofing", "兼容性", "伪装", nullptr, RenderAdvancedSettings },
        { "plugins", "兼容性", "启动", nullptr, RenderAdvancedSettings },
        { "shaders_limits", "兼容性", "着色器", nullptr, RenderAdvancedSettings },
        { "barriers", "兼容性", "资源屏障", nullptr, RenderAdvancedSettings },
        { "root_signatures", "兼容性", "根签名", nullptr, RenderAdvancedSettings },

        { "menu_scale", "菜单", "菜单缩放", [](RenderMenuContext& c) { RenderCard(c, RenderMenuScaleSettings); } },
        { "theme", "菜单", "主题与颜色", [](RenderMenuContext& c) { RenderCard(c, RenderThemeSettings); } },
        { "fps_overlay", "菜单", "FPS 覆盖层", [](RenderMenuContext& c) { RenderCard(c, RenderFpsOverlaySettings); } },
        { "keybinds", "菜单", "按键绑定", [](RenderMenuContext& c) { RenderCard(c, RenderKeybindSettings); } },
        { "custom_tab", "菜单", "自定义标签页", [](RenderMenuContext& c) { RenderCard(c, RenderCustomTabSettings); }, nullptr, false },

        { "active_quirks", "兼容性修正", "生效的兼容性修正", nullptr, RenderQuirksSettings },
        { "loaded_asi_plugins", "兼容性修正", "已加载 ASI 插件", nullptr, RenderQuirksSettings },

        { "status_system", "状态", "系统", nullptr, RenderStatusInfo },
        { "status_detected", "状态", "检测结果", nullptr, RenderStatusInfo },
        { "status_hooks", "状态", "挂钩", nullptr, RenderStatusInfo },
        { "status_init", "状态", "初始化标志（生效）", nullptr, RenderStatusInfo },
        { "logging", "状态", "日志", [](RenderMenuContext& c) { RenderCard(c, RenderLoggingSettings); } },
    };
    // clang-format on

    count = std::size(boxes);
    return boxes;
}

// Draws a single box, even when it's one of many drawn by the same function
void MenuCommon::RenderMenuBox(RenderMenuContext& ctx, const MenuBox& box)
{
    if (box.draw != nullptr)
    {
        box.draw(ctx);
        return;
    }

    ScopedCard::SetFilter(box.id);
    box.source(ctx);
    ScopedCard::SetFilter(nullptr);
}

// Ids are kept in a comma separated list
static bool IsBoxPinned(const std::string& pinned, const char* id)
{
    const size_t length = strlen(id);

    for (size_t pos = pinned.find(id); pos != std::string::npos; pos = pinned.find(id, pos + 1))
    {
        const bool startOk = pos == 0 || pinned[pos - 1] == ',';
        const bool endOk = pos + length == pinned.size() || pinned[pos + length] == ',';

        if (startOk && endOk)
            return true;
    }

    return false;
}

void MenuCommon::RenderCustomTab(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    const std::string pinned = config->CustomTabCards.value_or_default();

    size_t count = 0;
    const MenuBox* boxes = GetMenuBoxes(count);

    bool anyPinned = false;
    const float startY = ImGui::GetCursorPosY();

    for (size_t i = 0; i < count; i++)
    {
        if (!boxes[i].pinnable || !IsBoxPinned(pinned, boxes[i].id))
            continue;

        anyPinned = true;

        ImGui::PushID(boxes[i].id);

        // Explains why the upscaler settings are missing, the Upscaling tab draws it above its boxes
        if (strcmp(boxes[i].id, "upscaler") == 0)
        {
            auto feature = ctx.currentFeature;
            if (feature == nullptr || !feature->IsInited() || feature->IsFrozen())
                RenderCard(ctx, RenderUpscalerStateMessage);
        }

        RenderMenuBox(ctx, boxes[i]);
        ImGui::PopID();
    }

    // Boxes only show up when their feature is in use
    if (!anyPinned)
        ImGui::TextDisabled("尚未选择，请在 菜单 > 自定义标签页 中选择卡片");
    else if (ImGui::GetCursorPosY() == startY)
        ImGui::TextDisabled("当前没有可用的已选卡片");

    // Hint pinned to the bottom of the tab
    const float hintHeight = ImGui::GetFrameHeight();
    const float gap = ImGui::GetContentRegionAvail().y - hintHeight - ImGui::GetStyle().ItemSpacing.y;
    if (gap > 0.0f)
        ImGui::Dummy(ImVec2(0.0f, gap));
    else
        ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::SmallButton("自定义此标签页"))
        requestedMenuTab = "菜单";
    ImGui::PopStyleColor(2);
}

void MenuCommon::RenderCustomTabSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    SectionTitle("自定义标签页");

    ConfigCheckbox("显示自定义标签页", config->CustomTabEnabled,
                   "在列表顶部添加一个标签页，仅显示已选卡片");

    if (!config->CustomTabEnabled.value_or_default())
        return;

    ImGui::Spacing();

    if (auto ch = ScopedCollapsingHeader("要显示的卡片"); ch.IsHeaderOpen())
    {
        const std::string pinned = config->CustomTabCards.value_or_default();
        std::string newPinned;
        bool changed = false;

        size_t count = 0;
        const MenuBox* boxes = GetMenuBoxes(count);

        const char* lastTab = nullptr;
        bool tableOpen = false;

        for (size_t i = 0; i < count; i++)
        {
            const MenuBox& box = boxes[i];

            if (!box.pinnable)
                continue;

            if (lastTab == nullptr || strcmp(lastTab, box.tab) != 0)
            {
                if (tableOpen)
                    ImGui::EndTable();

                ImGui::TextDisabled("%s", box.tab);
                tableOpen = ImGui::BeginTable(box.tab, 2, ImGuiTableFlags_SizingStretchSame);
                lastTab = box.tab;
            }

            bool on = IsBoxPinned(pinned, box.id);

            if (tableOpen)
            {
                ImGui::TableNextColumn();

                ImGui::PushID(box.id);
                changed |= ImGui::Checkbox(box.name, &on);
                ImGui::PopID();
            }

            if (on)
            {
                if (!newPinned.empty())
                    newPinned += ',';

                newPinned += box.id;
            }
        }

        if (tableOpen)
            ImGui::EndTable();

        if (changed)
            config->CustomTabCards = newPinned;

        ImGui::Spacing();

        if (ImGui::Button("重置自定义标签页"))
            config->CustomTabCards.reset();
    }
}

void MenuCommon::RenderMainMenuTabs(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    using DrawFn = void (*)(RenderMenuContext&);

    struct Tab
    {
        const char* group; // Groups are split by a line in the sidebar
        const char* label;
        DrawFn draw = nullptr;                                     // Drawn above the boxes of the tab
        bool (*visible)(RenderMenuContext&) = nullptr;             // Tab hidden when returned false
        const char* (*displayLabel)(RenderMenuContext&) = nullptr; // Name shown in the sidebar
    };

    // Boxes of a tab come from GetMenuBoxes, by the label of the tab
    // clang-format off
    static const Tab tabs[] = {
        { "main", "自定义", RenderCustomTab },
        { "main", "超分", RenderUpscalerStateMessage },
        { "main", "帧生成" },
        { "main", "延迟" },
        { "system", "纹理", nullptr, [](RenderMenuContext& c) { return c.state.swapchainApi != Vulkan; } },
        { "system", "兼容性" },
        { "advanced", "菜单" },
        { "advanced", "兼容性修正", RenderQuirksSettings, [](RenderMenuContext& c) { return QuirksTabVisible(c.state); },
          [](RenderMenuContext& c) { return QuirksTabLabel(c.state); } },
        { "advanced", "状态" },
    };
    // clang-format on

    constexpr int customTab = 0;
    const bool customTabEnabled = config->CustomTabEnabled.value_or_default();

    // Tabs registered by plugins come after the built-in ones
    constexpr int builtinTabCount = (int) std::size(tabs);
    const auto pluginTabs = PluginApi::GetTabs();
    const int tabCount = builtinTabCount + (int) pluginTabs.size();

    auto isTabVisible = [&](int i)
    {
        if (i >= builtinTabCount)
            return true;
        if (i == customTab)
            return customTabEnabled;
        return tabs[i].visible == nullptr || tabs[i].visible(ctx);
    };

    if (requestedMenuTab != nullptr)
    {
        for (int i = 0; i < (int) std::size(tabs); i++)
        {
            if (strcmp(tabs[i].label, requestedMenuTab) == 0)
                selectedMenuTab = i;
        }

        requestedMenuTab = nullptr;
    }

    selectedMenuTab = std::clamp(selectedMenuTab, 0, tabCount - 1);

    // Selected tab can disappear, pick the first one that's visible
    if (!isTabVisible(selectedMenuTab))
    {
        for (int i = 0; i < builtinTabCount; i++)
        {
            if (isTabVisible(i))
            {
                selectedMenuTab = i;
                break;
            }
        }
    }

    const auto& style = ImGui::GetStyle();

    // Space kept free below for the frametime graph and the buttons
    const float footerHeight = FooterHeight();
    const float sidebarWidth = 130.0f * menuResScale;
    const float tabHeight = ImGui::GetFrameHeight() * 1.35f;

    // Sidebar -----------------------------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild("##MenuSidebar", ImVec2(sidebarWidth, -footerHeight),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    {
        const char* lastGroup = nullptr;

        for (int i = 0; i < tabCount; i++)
        {
            if (!isTabVisible(i))
                continue;

            const bool pluginTab = i >= builtinTabCount;
            const char* group = pluginTab ? "PLUGINS" : tabs[i].group;

            if (lastGroup != nullptr && strcmp(lastGroup, group) != 0)
                ImGui::Separator();

            lastGroup = group;

            const bool selected = selectedMenuTab == i;

            ImGui::PushID(i);
            if (ImGui::Selectable("##Tab", selected, 0, ImVec2(0.0f, tabHeight)))
                selectedMenuTab = i;
            ImGui::PopID();

            // Description comes from the plugin, so it isn't used as a format string
            if (pluginTab && !pluginTabs[i - builtinTabCount].description.empty() &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(pluginTabs[i - builtinTabCount].description.c_str());
                ImGui::EndTooltip();
            }

            // Label is drawn manually to center it vertically
            const ImVec2 rectMin = ImGui::GetItemRectMin();
            const ImVec2 rectMax = ImGui::GetItemRectMax();
            auto drawList = ImGui::GetWindowDrawList();

            const char* label = pluginTab                         ? pluginTabs[i - builtinTabCount].name.c_str()
                                : tabs[i].displayLabel != nullptr ? tabs[i].displayLabel(ctx)
                                                                  : tabs[i].label;

            // Long plugin names are cut at the edge of the sidebar
            drawList->PushClipRect(rectMin, rectMax, true);
            drawList->AddText(
                ImVec2(rectMin.x + 6.0f * menuResScale, rectMin.y + (tabHeight - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), label);
            drawList->PopClipRect();
        }

        // OptiPatcher status only visible when used
        const bool optiPatcher = state.isOptiPatcherSucceed;

        // Compact state of the upscaler and FG pinned to the bottom of the sidebar
        const float statusLines = 3.0f + (optiPatcher ? 1.0f : 0.0f);
        const float statusHeight = 1.0f + ImGui::GetTextLineHeightWithSpacing() * statusLines + style.ItemSpacing.y;
        const float gap = ImGui::GetContentRegionAvail().y - statusHeight - style.ItemSpacing.y;
        if (gap > 0.0f)
            ImGui::Dummy(ImVec2(0.0f, gap));

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("SR");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        if (currentFeature == nullptr || !currentFeature->IsInited())
            ImGui::TextDisabled("无");
        else if (currentFeature->IsFrozen())
            ImGui::TextColored(StatusWarn(), "空闲");
        else
        {
            const float renderWidth = (float) currentFeature->RenderWidth();
            const float upscaleRatio = renderWidth > 0.0f ? (float) currentFeature->TargetWidth() / renderWidth : 0.0f;

            ImGui::TextColored(StatusOk(), "%s (%.2f)", currentFeature->ShortName().c_str(), upscaleRatio);
        }

        ImGui::TextDisabled("FG");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        auto fg = state.CurrentFG();

        // Nvngx replacements don't use currentFG, have to use the DLSSG state detected from the game
        const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;

        if (activeNvngxFg != FGNvngxReplacement::None)
        {
            const char* nvngxName = activeNvngxFg == FGNvngxReplacement::Nukems   ? "Nukems"
                                    : activeNvngxFg == FGNvngxReplacement::Arturs ? "Enabler"
                                    : activeNvngxFg == FGNvngxReplacement::FFX    ? "FFX"
                                    : activeNvngxFg == FGNvngxReplacement::Custom ? state.activeFgNvngxCustom.c_str()
                                                                                  : "组合";
            const int detectedCount = state.dlssgDetectedInterpolationCount;

            if (detectedCount <= 0)
            {
                ImGui::TextDisabled("关");
            }

            else
            {
                const bool unsupported = detectedCount > Nvngx_FG::getMaxFakeFramesCount();
                ImGui::TextColored(unsupported ? StatusWarn() : StatusOk(), "%s %dx", nvngxName, detectedCount + 1);
            }
        }

        else if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
        {
            const UINT multiplier = fg->GetInterpolatedFrameCount() + 1;

            // XeFG already has multiplier in name when MFG detected
            std::string fgName = fg->Name();
            const std::string multiplierSuffix = StrFmt(" %ux", multiplier);
            if (fgName.ends_with(multiplierSuffix))
                fgName.erase(fgName.size() - multiplierSuffix.size());

            ImGui::TextColored(StatusOk(), "%s %ux", fgName.c_str(), multiplier);
        }
        else
            ImGui::TextDisabled("关");

        const bool dxgiSpoofing = config->DxgiSpoofing.value_or_default();
        const bool vkSpoofing = config->VulkanSpoofing.value_or_default();
        const bool vkExtSpoofing = config->VulkanExtensionSpoofing.value_or_default();

        ImGui::TextDisabled("伪装");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        if (vkSpoofing && vkExtSpoofing)
            ImGui::TextColored(StatusOk(), "VLK + Ext");
        else if (vkSpoofing)
            ImGui::TextColored(StatusOk(), "VLK");
        else if (vkExtSpoofing)
            ImGui::TextColored(StatusOk(), "VLK Ext");
        else if (dxgiSpoofing)
            ImGui::TextColored(StatusOk(), "DXGI");
        else
            ImGui::TextDisabled("否");

        if (optiPatcher)
        {
            ImGui::TextDisabled("OP");
            ImGui::SameLine(0.0f, 8.0f * menuResScale);
            ImGui::TextColored(StatusOk(), "是");
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Scrollable content of the tab -----------------------------
    ImGui::BeginChild("##MenuContent", ImVec2(0.0f, -footerHeight), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (selectedMenuTab >= builtinTabCount)
    {
        // Own ID per tab, so every tab remembers its scroll position
        ImGui::PushID(selectedMenuTab);
        ImGui::BeginChild("##MenuTabBody", ImVec2(0.0f, 0.0f));

        PluginApi::DrawTab(selectedMenuTab - builtinTabCount, menuResScale, toneMapColor);

        ImGui::EndChild();
        ImGui::PopID();
    }
    else
    {
        const Tab& tab = tabs[selectedMenuTab];

        // Own ID per tab, so every tab remembers its scroll position
        ImGui::PushID(selectedMenuTab);
        ImGui::BeginChild("##MenuTabBody", ImVec2(0.0f, 0.0f));

        if (tab.draw != nullptr)
            tab.draw(ctx);

        size_t boxCount = 0;
        const MenuBox* boxes = GetMenuBoxes(boxCount);
        DrawFn lastSource = nullptr;

        for (size_t i = 0; i < boxCount; i++)
        {
            if (strcmp(boxes[i].tab, tab.label) != 0)
                continue;

            // A function with several boxes is drawn once for all of them
            if (boxes[i].draw == nullptr)
            {
                if (boxes[i].source != lastSource)
                    boxes[i].source(ctx);
            }
            else
            {
                boxes[i].draw(ctx);
            }

            lastSource = boxes[i].source;
        }

        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MenuCommon::PlotLinesWithRange(const char* label, float (*values_getter)(void* data, int idx), void* data,
                                    int values_count, int values_offset, const char* overlay_text, ImVec2 graph_size,
                                    float padding)
{
    float v_min = FLT_MAX;
    float v_max = -FLT_MAX;
    for (int i = 0; i < plotWidth; i++)
    {
        const float v = values_getter(data, i);
        if (v != v) // Ignore NaN values
            continue;
        v_min = ImMin(v_min, v);
        v_max = ImMax(v_max, v);
    }

    ImGui::PlotEx(ImGuiPlotType_Lines, label, values_getter, data, values_count, values_offset, overlay_text, v_min,
                  v_max, graph_size);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("范围：%.2f ms - %.2f ms", v_min, v_max);
}

void MenuCommon::RenderFrameTimeGraph(RenderMenuContext& ctx)
{
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& menuResScale = ctx.menuResScale;
    auto& averageFrameTime = ctx.averageFrameTime;

    ImGui::Text("帧时间：%.2fms / %.1ffps（平均：%.1ffps）", frameTime, frameRate, 1000.f / averageFrameTime);

    PlotLinesWithRange(
        "##FrameTime",
        [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
        &gFrameTimes, plotWidth, 0, nullptr, ImVec2(0.0f, FooterPlotHeight()), 4.0f * menuResScale);
}

void MenuCommon::RenderUpscalerTimeGraph(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    if (!IsUpscalerActive(currentFeature))
        return;

    ImGui::Text("超分器 GPU 耗时：%.2fms（平均：%.2fms）", state.upscaleTimes.back(), averageUpscalerFT);

    PlotLinesWithRange(
        "##UpscalerTime",
        [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
        &gUpscalerTimes, plotWidth, 0, nullptr, ImVec2(0.0f, FooterPlotHeight()), 4.0f * menuResScale);
}

void MenuCommon::RenderMainMenuFooter(RenderMenuContext& ctx)
{
    ImGui::Separator();
    ImGui::Spacing();

    // Placed by hand instead of using a table or SameLine. Both of them carry the text baseline of the
    // first graph over to the second one, which made it sit a few pixels lower
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float graphWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
    const ImVec2 startPos = ImGui::GetCursorPos();

    ImGui::PushItemWidth(graphWidth);

    RenderFrameTimeGraph(ctx);
    const float endY = ImGui::GetCursorPosY();

    // The indent is needed because the cursor goes back to the left edge after every item
    ImGui::SetCursorPosY(startPos.y);
    ImGui::Indent(graphWidth + spacing);
    RenderUpscalerTimeGraph(ctx);
    ImGui::Unindent(graphWidth + spacing);

    ImGui::PopItemWidth();

    ImGui::SetCursorPosY(endY);

    RenderMainMenuBottomBar(ctx);
}

void MenuCommon::RenderShaderTimes(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& currentFeature = ctx.currentFeature;

    if (!IsUpscalerActive(currentFeature) || state.detailedGpuTimes.empty())
        return;

    SectionTitle("各着色器 GPU 耗时");

    if (ImGui::BeginTable("ShaderTimes", 2, ImGuiTableFlags_SizingFixedSame))
    {
        bool hasExtra = false;

        for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
        {
            if (!includedInUpscalerTime)
            {
                hasExtra = true;
                continue;
            }

            auto formattedTime = StrFmt("%7.2f ms", time);

            ImGui::TableNextColumn();
            ImGui::Text("%s", name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", formattedTime.c_str());
        }

        if (hasExtra)
        {
            ImGui::TableNextRow();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("额外着色器：");
            ImGui::TableNextColumn();
            ImGui::TextDisabled("");
            for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
            {
                if (includedInUpscalerTime)
                    continue;

                auto formattedTime = StrFmt("%7.2f ms", time);

                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", formattedTime.c_str());
            }
        }

        ImGui::EndTable();
    }
}

void MenuCommon::RenderMainMenuBottomBar(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;

    // BOTTOM LINE ---------------
    if (ImGui::Button("打开 Wiki"))
    {
        auto pIO = &ImGui::GetPlatformIO();
        auto ctx = ImGui::GetCurrentContext();
        pIO->Platform_OpenInShellFn(ctx, "https://github.com/optiscaler/OptiScaler/wiki");
    }
    ShowTooltip("兼容性列表、已知问题与帧生成选项说明");

    ImGui::SameLine();

    const auto saveLabel = "保存设置";
    const auto closeLabel = "关闭";

    auto& style = ImGui::GetStyle();
    float rightButtonsWidth = ImGui::CalcTextSize(saveLabel, nullptr, true).x +
                              ImGui::CalcTextSize(closeLabel, nullptr, true).x + style.FramePadding.x * 4.0f + 6.0f;

    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - rightButtonsWidth);

    if (ImGui::Button(saveLabel))
        config->SaveIni();

    ImGui::SameLine(0.0f, 6.0f);

    if (ImGui::Button(closeLabel))
    {
        _isVisible = false;
        hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
        io.BackendFlags &= 30;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

        _showMipmapCalcWindow = false;
        _showHudlessWindow = false;
        io.MouseDrawCursor = false;
        io.WantCaptureKeyboard = false;
        io.WantCaptureMouse = false;
    }
}

void MenuCommon::RenderMipmapBiasWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;

    // Metrics window (for debug)
    // ImGui::ShowMetricsWindow();

    // Mipmap calculation window
    if (_showMipmapCalcWindow && currentFeature != nullptr && !currentFeature->IsFrozen() && currentFeature->IsInited())
    {
        auto posX = (io.DisplaySize.x - 450.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 200.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 450.0f, 200.0f }, ImGuiCond_FirstUseEver);

        if (_displayWidth == 0)
        {
            if (config->OutputScalingEnabled.value_or_default())
            {
                _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                      config->OutputScalingMultiplier.value_or_default());
            }
            else
            {
                _displayWidth = currentFeature->DisplayWidth();
            }

            _renderWidth = static_cast<uint32_t>(_displayWidth / 3.0f);
            _mipmapUpscalerQuality = 0;
            _mipmapUpscalerRatio = 3.0f;
            _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
        }

        if (ImGui::Begin("Mipmap 偏移", nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            if (ImGui::InputScalar("显示宽度", ImGuiDataType_U32, &_displayWidth, NULL, NULL, "%u"))
            {
                if (_displayWidth <= 0)
                {
                    if (config->OutputScalingEnabled.value_or_default())
                    {
                        _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                              config->OutputScalingMultiplier.value_or_default());
                    }
                    else
                    {
                        _displayWidth = currentFeature->DisplayWidth();
                    }
                }

                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            const char* q[] = { "极致性能", "性能", "均衡", "质量", "极致质量", "DLAA" };
            float fr[] = { 3.0f, 2.0f, 1.7f, 1.5f, 1.3f, 1.0f };
            auto configQ = _mipmapUpscalerQuality;

            const char* selectedQ = q[configQ];

            ImGui::BeginDisabled(config->UpscaleRatioOverrideEnabled.value_or_default());

            if (ImGui::BeginCombo("超分质量", selectedQ))
            {
                for (int n = 0; n < 6; n++)
                {
                    if (ImGui::Selectable(q[n], (_mipmapUpscalerQuality == n)))
                    {
                        _mipmapUpscalerQuality = n;

                        float ov = -1.0f;

                        if (config->QualityRatioOverrideEnabled.value_or_default())
                        {
                            switch (n)
                            {
                            case 0:
                                ov = config->QualityRatio_UltraPerformance.value_or(-1.0f);
                                break;

                            case 1:
                                ov = config->QualityRatio_Performance.value_or(-1.0f);
                                break;

                            case 2:
                                ov = config->QualityRatio_Balanced.value_or(-1.0f);
                                break;

                            case 3:
                                ov = config->QualityRatio_Quality.value_or(-1.0f);
                                break;

                            case 4:
                                ov = config->QualityRatio_UltraQuality.value_or(-1.0f);
                                break;
                            }
                        }

                        if (ov > 0.0f)
                            _mipmapUpscalerRatio = ov;
                        else
                            _mipmapUpscalerRatio = fr[n];

                        _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                        _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::EndDisabled();

            auto minLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
            auto maxLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;
            if (ImGui::SliderFloat("超分倍率", &_mipmapUpscalerRatio, minLimit, maxLimit, "%.2f"))
            {
                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            if (ImGui::InputScalar("渲染宽度", ImGuiDataType_U32, &_renderWidth, NULL, NULL, "%u"))
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);

            ImGui::SliderFloat("Mipmap 偏移", &_mipBiasCalculated, -15.0f, 0.0f, "%.6f");

            // BOTTOM LINE
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SameLine();
            ImGui::Spacing();

            constexpr float spacing = 6.0f;
            auto textSize = ImGui::CalcTextSize("使用此值");
            textSize += ImGui::CalcTextSize("关闭");
            textSize.x += ImGui::GetStyle().FramePadding.x * 5.0f + spacing; // 2 sides * 2 buttons + 1

            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textSize.x);

            if (ImGui::Button("使用此值"))
            {
                _mipBias = _mipBiasCalculated;
                _showMipmapCalcWindow = false;
            }

            ImGui::SameLine(0.0f, spacing);

            if (ImGui::Button("关闭"))
                _showMipmapCalcWindow = false;

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::End();
        }
    }
}

void MenuCommon::RenderHudlessResourcesWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    auto fg = state.currentFG;
    if (_showHudlessWindow && config->FGHUDFix.value_or_default() && fg != nullptr && fg->IsActive())
    {
        auto posX = (io.DisplaySize.x - 400.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 300.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 400.0f, 300.0f });

        if (ImGui::Begin("HUDless 资源列表", nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            int btnCount = 100;

            if (ImGui::BeginTable("HUDlessTable", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthFixed);

                ankerl::unordered_dense::map<void*, CapturedHudlessInfo>::iterator it;

                // Render threads add and remove entries
                std::scoped_lock hudlessLock(state.capturedHudlessesMutex);

                for (it = state.capturedHudlesses.begin(); it != state.capturedHudlesses.end(); it++)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    ImGui::Text("%08x, %s->%s, 数量: %llu, %s", (size_t) it->first,
                                GetSourceString(it->second.captureInfo & 0xFF).c_str(),
                                GetDispatchString(it->second.captureInfo & 0xFF00).c_str(), it->second.usageCount,
                                it->second.enabled ? "活跃" : "被动");

                    ImGui::TableSetColumnIndex(1);

                    btnCount++;
                    std::string text;

                    if (it->second.enabled)
                        text = StrFmt("禁用##%d", btnCount);
                    else
                        text = StrFmt("启用##%d", btnCount);

                    if (ImGui::Button(text.c_str()))
                    {
                        LOG_DEBUG("HUDless {:X}：{}", (size_t) it->first,
                                  it->second.enabled ? "正在禁用" : "正在启用");
                        it->second.enabled = !it->second.enabled;
                    }
                }

                ImGui::EndTable();
            }

            if (ImGui::Button("清除##4"))
            {
                LOG_DEBUG("正在清除已捕获的 HUDless 资源");
                state.clearCapturedHudlesses = true;
            }

            ImGui::SameLine(0.0f, 8.0f);

            if (ImGui::Button("关闭##4"))
                _showHudlessWindow = false;

            ImGui::End();
        }
    }
}

// API | GPU | Input, above the tabs
static void RenderGameInfoRow(State& state, const GpuInformation& primaryGpu)
{
    // Game name is in the window title
    ScopedCard card;
    std::string line = StrFmt("%s%s | %s | Input: %s", ApiName(GameApi(state)),
                              IdentifyGpu::gameUsesDxvk()          ? " (DXVK)"
                              : IdentifyGpu::gameUsesVkd3dProton() ? " (VKD3D)"
                                                                   : "",
                              primaryGpu.name.c_str(), ApiUpscalerInputName(state.currentInputApiName).c_str());

    const float avail = ImGui::GetContentRegionAvail().x;
    const float textWidth = ImGui::CalcTextSize(line.c_str()).x;
    if (textWidth < avail)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textWidth) * 0.5f);
        ImGui::TextUnformatted(line.c_str());
    }
    else
    {
        ImGui::TextWrapped("%s", line.c_str());
    }
}

void MenuCommon::RenderMainMenuWindow(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;
    auto& io = ctx.io;

    if (!_isVisible)
        return;

    // Check for GPU support once and reuse the result in all menu sections.
    // DXVK might call Vulkan device creation, which would destroy our objects.
    State::Instance().vulkanSkipHooks = true;
    ctx.primaryGpu =
        std::make_unique<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>>(IdentifyGpu::getPrimaryGpu());
    State::Instance().vulkanSkipHooks = false;

    // Overlay font
    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(menuResScale * fontSize));

    // If overlay is not visible frame needs to be inited
    if (!frameTimesCalculated)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
    }

    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoSavedSettings;
    flags |= ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_AlwaysAutoResize;

    // Fixed width, only the tab content scrolls. Keeps the menu from growing or shrinking when tabs change
    // Height can be changed by the user with the resize grip and is stored unscaled in the config
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    const float menuWidth = std::min(680.0f * menuResScale, io.DisplaySize.x - 20.0f);
    const float maxMenuHeight = io.DisplaySize.y - 20.0f;
    const float minMenuHeight = std::min(300.0f * menuResScale, maxMenuHeight);
    const float menuHeight =
        std::clamp(config->MenuHeight.value_or(815.0f) * menuResScale, minMenuHeight, maxMenuHeight);

    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(menuWidth, minMenuHeight), ImVec2(menuWidth, maxMenuHeight));

    // Main menu window, fixed ID as the game name can show up later
    windowTitle = StrFmt("%s - %s###OptiScalerMenu", BuildInfo::ProductName(),
                         state.gameName.empty() ? state.gameExe.c_str() : state.gameName.c_str());

    if (ImGui::Begin(windowTitle.c_str(), NULL, mainFlags))
    {
        // Update and config notices shown above the tabs.
        RenderMainMenuHeaderMessages(ctx);

        // Game name, API, GPU and input, above the tabs.
        RenderGameInfoRow(state, *ctx.primaryGpu);

        // Tabs on the left, tab content on the right.
        RenderMainMenuTabs(ctx);

        // Graphs and buttons, always visible regardless of the selected tab.
        RenderMainMenuFooter(ctx);

        auto winSize = ImGui::GetWindowSize();
        auto winPos = ImGui::GetWindowPos();

        // Menu was resized by the user
        if (std::abs(winSize.y - menuHeight) > 0.5f)
            config->MenuHeight = std::round(winSize.y / menuResScale);

        if (lastPosition.x < -900.0f || (lastPosition.x >= winPos.x - 1.0f && lastPosition.y >= winPos.y - 1.0f &&
                                         lastPosition.x <= winPos.x + 1.0f && lastPosition.y <= winPos.y + 1.0f))
        {
            float posX;
            float posY;

            posX = ((float) io.DisplaySize.x - winSize.x) / 2.0f;
            posY = ((float) io.DisplaySize.y - winSize.y) / 2.0f;

            // don't position menu outside of screen
            if (posX < 0.0 || posY < 0.0)
            {
                posX = 50;
                posY = 50;
            }

            ImGui::SetWindowPos(ImVec2 { posX, posY });
            lastPosition.x = posX;
            lastPosition.y = posY;
        }

        // Blurred game image behind the window, the semi transparent window background tints it
        if (_bgBlurTexture != ImTextureID_Invalid && config->MenuBlur.value_or_default() &&
            ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).w < 1.0f)
        {
            const ImVec2 blurMin = ImGui::GetWindowPos();
            const ImVec2 blurMax = ImVec2(blurMin.x + ImGui::GetWindowWidth(), blurMin.y + ImGui::GetWindowHeight());
            const ImVec2 uvMul = ImVec2(_bgBlurUVScale.x / io.DisplaySize.x, _bgBlurUVScale.y / io.DisplaySize.y);

            ImGui::GetBackgroundDrawList()->AddImageRounded(
                _bgBlurTexture, blurMin, blurMax, ImVec2(blurMin.x * uvMul.x, blurMin.y * uvMul.y),
                ImVec2(blurMax.x * uvMul.x, blurMax.y * uvMul.y), IM_COL32_WHITE, ImGui::GetStyle().WindowRounding);

            _bgBlurUsed = true;
        }

        ImGui::End();
    }

    // Detached utility windows owned by the main menu.
    RenderMipmapBiasWindow(ctx, flags);
    RenderHudlessResourcesWindow(ctx, flags);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void KeyUp(UINT vKey)
{
    inputMenu = vKey == Config::Instance()->ShortcutKey.value_or_default();
    inputFps = vKey == Config::Instance()->FpsShortcutKey.value_or_default();
    inputFG = vKey == Config::Instance()->FGShortcutKey.value_or_default();
    inputFpsCycle = vKey == Config::Instance()->FpsCycleShortcutKey.value_or_default();
}

bool MenuCommon::RenderMenu()
{
    if (!_isInited)
        return false;

    RenderMenuContext ctx { State::Instance(), Config::Instance(), ImGui::GetIO() };
    ctx.now = Util::MillisecondsNow();
    ctx.currentFeature = ctx.state.currentFeature;

    // Applied here, the feature isn't safe to read from the plugin's thread, kept until there is one
    if (ctx.currentFeature != nullptr && PluginApi::TakeReInitRequest())
        ReInitUpscaler();

    // 1) Collect timing and input state before any ImGui drawing.
    UpdateRenderTiming(ctx);
    UpdateMenuInputMode(ctx);
    HandleMenuShortcuts(ctx);

    // 2) Prepare one-shot notifications and start a new ImGui frame only when needed.
    UpdateVersionAndStartupNotifications(ctx);
    BeginMenuFrameIfNeeded(ctx);
    OptiInput::EndFrame(_isVisible);

    // 3) Draw lightweight overlay windows first, preserving the original order.
    ctx.menuResScale = MenuResolutionScale(ctx.io);

    // The overlays share the style with the menu, so it's scaled before them even when the menu is closed
    if (ctx.newFrame && lastMenuScale != ctx.menuResScale)
    {
        lastMenuScale = ctx.menuResScale;

        // if UI scale is changed rescale the style
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiStyle styleold = style; // Backup colors
        style = ImGuiStyle();        // IMPORTANT: ScaleAllSizes will change the original size,
                                     // so we should reset all style config

        ApplyThemeStyle();

        style.ScaleAllSizes(ctx.menuResScale);
        style.MouseCursorScale = 1.0f;
        CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors
    }

    RenderSplashWindow(ctx);
    RenderNotifications(ctx);
    UpdateFrameTimeAverages(ctx);
    RenderPerformanceOverlay(ctx);

    // 4) Draw the full settings menu last so popups and child windows keep their existing behavior.
    RenderMainMenuWindow(ctx);

    if (ctx.newFrame)
        ImGui::EndFrame();

    return ctx.newFrame;
}

void MenuCommon::Init(HWND InHwnd, bool isUWP)
{
    // Reset shutdown flag in case of re-init
    State::Instance().isShuttingDown = false;

    HWND oldHandle = nullptr;

    if (_handle != nullptr)
    {
        oldHandle = _handle;
        LOG_DEBUG("Old Handle: {:X}, ImGui Handle: {:X}", (size_t) oldHandle,
                  (size_t) ImGui::GetMainViewport()->PlatformHandleRaw);
    }

    _handle = InHwnd;
    _isVisible = false;
    _isUWP = isUWP;
    lastPosition = { -1000.0f, -1000.0f };

    LOG_DEBUG("Handle: {0:X}", (size_t) _handle);

    // In case d3d12 wasn't yet used up to this point, try to update GPU info late here
    IdentifyGpu::updateD3d12Capabilities();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
    io.BackendFlags &= 30;
    io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
    io.WantSetMousePos = _isVisible;

    io.IniFilename = io.LogFilename = nullptr;

    bool initResult = false;

    if (io.BackendPlatformUserData == nullptr)
    {
        if (!isUWP)
        {
            initResult = ImGui_ImplWin32_Init(InHwnd);
            LOG_DEBUG("ImGui_ImplWin32_Init result: {0}", initResult);
        }
        else
        {
            initResult = ImGui_ImplUwp_Init(InHwnd);
            ImGui_BindUwpKeyUp(KeyUp);
            LOG_DEBUG("ImGui_ImplUwp_Init result: {0}", initResult);
        }
    }

    if (io.Fonts->Fonts.empty() && Config::Instance()->UseHQFont.value_or_default())
    {
        ImFontAtlas* atlas = io.Fonts;
        atlas->Clear();

        // This automatically becomes the next default font
        ImFontConfig fontConfig;

        if (Config::Instance()->FontSize.has_value())
            fontSize = Config::Instance()->FontSize.value();

        if (Config::Instance()->TTFFontPath.has_value())
        {
            io.FontDefault =
                atlas->AddFontFromFileTTF(wstring_to_string(Config::Instance()->TTFFontPath.value()).c_str(), fontSize,
                                          &fontConfig, io.Fonts->GetGlyphRangesDefault());
        }
        else
        {
            io.FontDefault = atlas->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                         fontSize, &fontConfig);
        }
    }

    // [CN] Merge the embedded CJK subset font so the Chinese UI renders correctly.
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

    if (!Config::Instance()->OverlayMenu.value_or_default())
    {
        _hdrTonemapApplied = false;
    }

    DWORD hwndPid = 0;
    DWORD hwndTid = GetWindowThreadProcessId(_handle, &hwndPid);

    LOG_DEBUG("HWND: {:X}, IsWindow: {}, HWND PID: {}, Current PID: {}, HWND TID: {}, Current TID: {}",
              (ULONG64) _handle, IsWindow(_handle), hwndPid, GetCurrentProcessId(), hwndTid, GetCurrentThreadId());

    OptiInput::Initialize(_handle, isUWP);

    ApplyThemeStyle();
    _isInited = true;
}

void MenuCommon::Shutdown()
{
    if (!MenuCommon::_isInited)
        return;

    // if (_oWndProc != nullptr)
    //{
    //     auto handle = (HWND) ImGui::GetMainViewport()->PlatformHandleRaw;
    //     SetLastError(0);
    //     auto restoreResult = SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR) _oWndProc);
    //     auto error = GetLastError();

    //    if (restoreResult == 0 && error != 0)
    //    {
    //        LOG_ERROR("Failed to restore old WndProc. Error: {:X}", error);
    //    }

    //    _oWndProc = nullptr;
    //}

    if (!_isUWP)
        ImGui_ImplWin32_Shutdown();
    else
        ImGui_ImplUwp_Shutdown();

    ImGui::DestroyContext();

    // The font was in the destroyed context's atlas
    gOverlayFont = nullptr;
    gOverlayFontName.clear();
    gOverlayLetterSpacing = 0.0f;

    _handle = nullptr;
    _isInited = false;
    _isVisible = false;
}

void MenuCommon::HideMenu()
{
    if (!_isVisible)
        return;

    _isVisible = false;

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    _showMipmapCalcWindow = false;
    _showHudlessWindow = false;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
}
