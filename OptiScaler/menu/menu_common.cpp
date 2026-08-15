#include "pch.h"
#include "menu_common.h"

#include "font/Hack_Compressed.h"
#include "font/CJK_Compressed.h"

#include <proxies/XeSS_Proxy.h>
#include <proxies/XeFG_Proxy.h>
#include <proxies/FfxApi_Proxy.h>

#include <inputs/FG/DLSSG_Mod.h>

#include <fsr4/FSR4Upgrade.h>

#include <nvapi/fakenvapi.h>
#include <hooks/Reflex_Hooks.h>

#include <version_check.h>

#include <imgui/imgui_internal.h>

#include <mutex>
#include <cstdarg>

#include <array>
#include <chrono>

#define MARK_ALL_BACKENDS_CHANGED()                                                                                    \
    for (auto& singleChangeBackend : State::Instance().changeBackend)                                                  \
        singleChangeBackend.second = true;

static float fontSize = 14.0f; // just changing this doesn't make other elements scale ideally
static ImVec2 overlaySize(0.0f, 0.0f);
static ImVec2 overlayPosition(-1000.0f, -1000.0f);
static bool _hdrTonemapApplied = false;
static ImVec4 SdrColors[ImGuiCol_COUNT];
static bool receivingWmInputs = false;
static bool inputMenu = false;
static bool inputFG = false;
static bool inputFps = false;
static bool inputFpsCycle = false;
static bool inputManual = false;
static bool hasGamepad = false;
static bool fsr31InitTried = false;
static bool xefgInitTried = false;
static std::string windowTitle;
static std::string selectedUpscalerName = "";
static std::string currentBackend = "";
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
                                               "这不是你要找的分辨率。",
                                               "飞向无限……关掉光追的那种。",
                                               "我对这个帧节奏有种不祥的预感。",
                                               "独自前行很危险——带上这个超分器",
                                               "超分到面目全非。",
                                               "相信过程，无视闪烁。",
                                               "真正的假帧。经过认证。",
                                               "性能的幻象，臻于完美。",
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
                                               "深度学习糊弄采样 5",
                                               "2D AI 滤镜，仅需 2 张 5090 驱动",
                                               "神经糊弄采样搭配 DLSS5",
                                               "DLSS 5——糊弄的正确方式",
                                               "每当我以为退坑了，他们又把我拉回来超分",
                                               "怎么去掉这些土味消息？！",
                                               "<在这里填入你的搞笑文字>" };

static ImVec2 updateNoticePosition(-1000.0f, -1000.0f);
static ImVec2 updateNoticeSize(0.0f, 0.0f);
static double updateNoticeStart = 0.0;
static double updateNoticeLimit = 0.0;
static bool updateNoticeVisible = false;
static std::string updateNoticeTag;
static std::string updateNoticeUrl;
static float lastMenuScale = 0.0f;
static CustomOptional<uint32_t> comboPreset { 0 };
static int lastKey = 0;

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

struct FsExistsCache
{
    std::wstring lastPath;
    bool cached { false };
    std::chrono::steady_clock::time_point nextRefresh { std::chrono::steady_clock::time_point::min() };
    std::chrono::milliseconds interval { 2000 };

    bool Get(const std::wstring& path)
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

static FsExistsCache gExists;

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

bool IsKeyReleasedOnce(int vk)
{
    static bool previousDown[256] {};

    if (vk <= 0 || vk >= 256)
        return false;

    bool isDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool released = previousDown[vk] && !isDown;

    previousDown[vk] = isDown;

    return released;
}

void UpdateManualInput(HWND targetHwnd)
{
    ImGuiIO& io = ImGui::GetIO();

    // Only capture input when target window is foreground
    HWND foreground = GetForegroundWindow();
    bool focused = foreground == targetHwnd;

    io.AddFocusEvent(focused);

    if (!focused)
    {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        return;
    }

    const auto config = Config::Instance();

    auto CheckShortcut = [&](int vk, bool& inputFlag, const char* logMessage)
    {
        if (inputFlag)
            return;

        if (vk <= 0 || vk >= 256)
            return;

        if (IsKeyReleasedOnce(vk))
        {
            lastKey = vk;
            receivingWmInputs = false;
            inputFlag = true;
            LOG_DEBUG("{}", logMessage);
        }
    };

    CheckShortcut(config->ShortcutKey.value_or_default(), inputMenu, "Menu key pressed, will be switching menu");

    CheckShortcut(config->FpsShortcutKey.value_or_default(), inputFps, "Menu key pressed, will be switching FPS");

    CheckShortcut(config->FGShortcutKey.value_or_default(), inputFG, "Menu key pressed, will be switching FG mode");

    CheckShortcut(config->FpsCycleShortcutKey.value_or_default(), inputFpsCycle,
                  "Menu key pressed, will be switching FPS mode");

    // Mouse position
    POINT cursorPos {};
    GetCursorPos(&cursorPos);

    POINT clientPos = cursorPos;
    ScreenToClient(targetHwnd, &clientPos);

    io.AddMousePosEvent(static_cast<float>(clientPos.x), static_cast<float>(clientPos.y));

    // Mouse buttons
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(3, (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0);
    io.AddMouseButtonEvent(4, (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0);

    // Common keyboard keys
    auto AddKey = [&](ImGuiKey key, int vk) { io.AddKeyEvent(key, (GetAsyncKeyState(vk) & 0x8000) != 0); };

    AddKey(ImGuiKey_Tab, VK_TAB);
    AddKey(ImGuiKey_LeftArrow, VK_LEFT);
    AddKey(ImGuiKey_RightArrow, VK_RIGHT);
    AddKey(ImGuiKey_UpArrow, VK_UP);
    AddKey(ImGuiKey_DownArrow, VK_DOWN);
    AddKey(ImGuiKey_PageUp, VK_PRIOR);
    AddKey(ImGuiKey_PageDown, VK_NEXT);
    AddKey(ImGuiKey_Home, VK_HOME);
    AddKey(ImGuiKey_End, VK_END);
    AddKey(ImGuiKey_Insert, VK_INSERT);
    AddKey(ImGuiKey_Delete, VK_DELETE);
    AddKey(ImGuiKey_Backspace, VK_BACK);
    AddKey(ImGuiKey_Space, VK_SPACE);
    AddKey(ImGuiKey_Enter, VK_RETURN);
    AddKey(ImGuiKey_Escape, VK_ESCAPE);

    AddKey(ImGuiKey_LeftCtrl, VK_LCONTROL);
    AddKey(ImGuiKey_LeftShift, VK_LSHIFT);
    AddKey(ImGuiKey_LeftAlt, VK_LMENU);
    AddKey(ImGuiKey_RightCtrl, VK_RCONTROL);
    AddKey(ImGuiKey_RightShift, VK_RSHIFT);
    AddKey(ImGuiKey_RightAlt, VK_RMENU);

    // Letters
    for (int vk = 'A'; vk <= 'Z'; vk++)
    {
        io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A')), (GetAsyncKeyState(vk) & 0x8000) != 0);
    }

    // Numbers
    for (int vk = '0'; vk <= '9'; vk++)
    {
        io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0')), (GetAsyncKeyState(vk) & 0x8000) != 0);
    }
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

void MenuCommon::ShowHelpMarker(const char* tip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    ShowTooltip(tip);
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
    if (State::Instance().currentFeature->Name() == "DLSSD")
        State::Instance().newBackend = "dlssd";
    else
        State::Instance().newBackend = currentBackend;

    MARK_ALL_BACKENDS_CHANGED();
}

void MenuCommon::SeparatorWithHelpMarker(const char* label, const char* tip)
{
    auto marker = "(?) ";
    ImGui::SeparatorTextEx(0, label, ImGui::FindRenderedTextEnd(label),
                           ImGui::CalcTextSize(marker, ImGui::FindRenderedTextEnd(marker)).x);
    ShowHelpMarker(tip);
}

LRESULT MenuCommon::hkSendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (_isVisible && Msg == 0x0020)
        return TRUE;
    else
        return pfn_SendMessageW(hWnd, Msg, wParam, lParam);
}

BOOL MenuCommon::hkSetPhysicalCursorPos(int x, int y)
{
    if (_isVisible)
        return TRUE;
    else
        return pfn_SetPhysicalCursorPos(x, y);
}

BOOL MenuCommon::hkGetPhysicalCursorPos(LPPOINT lpPoint)
{
    if (_isVisible)
    {
        lpPoint->x = _lastPoint.x;
        lpPoint->y = _lastPoint.y;
        return TRUE;
    }
    else
        return pfn_GetCursorPos(lpPoint);
}

BOOL MenuCommon::hkSetCursorPos(int x, int y)
{
    if (_isVisible)
        return TRUE;
    else
        return pfn_SetCursorPos(x, y);
}

BOOL MenuCommon::hkClipCursor(RECT* lpRect)
{
    if (_isVisible)
        return TRUE;
    else
    {
        return pfn_ClipCursor(lpRect);
    }
}

void MenuCommon::hkmouse_event(DWORD dwFlags, DWORD dx, DWORD dy, DWORD dwData, ULONG_PTR dwExtraInfo)
{
    if (_isVisible)
        return;
    else
        pfn_mouse_event(dwFlags, dx, dy, dwData, dwExtraInfo);
}

UINT MenuCommon::hkSendInput(UINT cInputs, LPINPUT pInputs, int cbSize)
{
    if (_isVisible)
        return TRUE;
    else
        return pfn_SendInput(cInputs, pInputs, cbSize);
}

void MenuCommon::AttachHooks()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Detour the functions
    pfn_SetPhysicalCursorPos =
        reinterpret_cast<PFN_SetCursorPos>(DetourFindFunction("user32.dll", "SetPhysicalCursorPos"));
    pfn_SetCursorPos = reinterpret_cast<PFN_SetCursorPos>(DetourFindFunction("user32.dll", "SetCursorPos"));
    pfn_ClipCursor = reinterpret_cast<PFN_ClipCursor>(DetourFindFunction("user32.dll", "ClipCursor"));
    pfn_mouse_event = reinterpret_cast<PFN_mouse_event>(DetourFindFunction("user32.dll", "mouse_event"));
    pfn_SendInput = reinterpret_cast<PFN_SendInput>(DetourFindFunction("user32.dll", "SendInput"));
    pfn_SendMessageW = reinterpret_cast<PFN_SendMessageW>(DetourFindFunction("user32.dll", "SendMessageW"));

    if (pfn_SetPhysicalCursorPos && (pfn_SetPhysicalCursorPos != pfn_SetCursorPos))
        pfn_SetPhysicalCursorPos_hooked =
            (DetourAttach(&(PVOID&) pfn_SetPhysicalCursorPos, hkSetPhysicalCursorPos) == 0);

    if (pfn_SetCursorPos)
        pfn_SetCursorPos_hooked = (DetourAttach(&(PVOID&) pfn_SetCursorPos, hkSetCursorPos) == 0);

    if (pfn_ClipCursor)
        pfn_ClipCursor_hooked = (DetourAttach(&(PVOID&) pfn_ClipCursor, hkClipCursor) == 0);

    if (pfn_mouse_event)
        pfn_mouse_event_hooked = (DetourAttach(&(PVOID&) pfn_mouse_event, hkmouse_event) == 0);

    if (pfn_SendInput)
        pfn_SendInput_hooked = (DetourAttach(&(PVOID&) pfn_SendInput, hkSendInput) == 0);

    if (pfn_SendMessageW)
        pfn_SendMessageW_hooked = (DetourAttach(&(PVOID&) pfn_SendMessageW, hkSendMessageW) == 0);

    auto detourResult = DetourTransactionCommit();
    if (detourResult != NO_ERROR)
    {
        LOG_ERROR("DetourTransactionCommit failed: {:X}", detourResult);

        pfn_SetPhysicalCursorPos = nullptr;
        pfn_SetCursorPos = nullptr;
        pfn_ClipCursor = nullptr;
        pfn_mouse_event = nullptr;
        pfn_SendInput = nullptr;
        pfn_SendMessageW = nullptr;

        pfn_SetPhysicalCursorPos_hooked = false;
        pfn_SetCursorPos_hooked = false;
        pfn_ClipCursor_hooked = false;
        pfn_mouse_event_hooked = false;
        pfn_SendInput_hooked = false;
        pfn_SendMessageW_hooked = false;
    }
}

void MenuCommon::DetachHooks()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (pfn_SetPhysicalCursorPos_hooked)
        DetourDetach(&(PVOID&) pfn_SetPhysicalCursorPos, hkSetPhysicalCursorPos);

    if (pfn_SetCursorPos_hooked)
        DetourDetach(&(PVOID&) pfn_SetCursorPos, hkSetCursorPos);

    if (pfn_ClipCursor_hooked)
        DetourDetach(&(PVOID&) pfn_ClipCursor, hkClipCursor);

    if (pfn_mouse_event_hooked)
        DetourDetach(&(PVOID&) pfn_mouse_event, hkmouse_event);

    if (pfn_SendInput_hooked)
        DetourDetach(&(PVOID&) pfn_SendInput, hkSendInput);

    if (pfn_SendMessageW_hooked)
        DetourDetach(&(PVOID&) pfn_SendMessageW, hkSendMessageW);

    auto detourResult = DetourTransactionCommit();
    if (detourResult != NO_ERROR)
    {
        LOG_ERROR("DetourTransactionCommit failed: {:X}", detourResult);
    }
    else
    {
        pfn_SetPhysicalCursorPos_hooked = false;
        pfn_SetCursorPos_hooked = false;
        pfn_ClipCursor_hooked = false;
        pfn_mouse_event_hooked = false;
        pfn_SendInput_hooked = false;
        pfn_SendMessageW_hooked = false;

        pfn_SetPhysicalCursorPos = nullptr;
        pfn_SetCursorPos = nullptr;
        pfn_ClipCursor = nullptr;
        pfn_mouse_event = nullptr;
        pfn_SendInput = nullptr;
        pfn_SendMessageW = nullptr;
    }
}

ImGuiKey MenuCommon::ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
{
    switch (wParam)
    {
    case VK_TAB:
        return ImGuiKey_Tab;
    case VK_LEFT:
        return ImGuiKey_LeftArrow;
    case VK_RIGHT:
        return ImGuiKey_RightArrow;
    case VK_UP:
        return ImGuiKey_UpArrow;
    case VK_DOWN:
        return ImGuiKey_DownArrow;
    case VK_PRIOR:
        return ImGuiKey_PageUp;
    case VK_NEXT:
        return ImGuiKey_PageDown;
    case VK_HOME:
        return ImGuiKey_Home;
    case VK_END:
        return ImGuiKey_End;
    case VK_INSERT:
        return ImGuiKey_Insert;
    case VK_DELETE:
        return ImGuiKey_Delete;
    case VK_BACK:
        return ImGuiKey_Backspace;
    case VK_SPACE:
        return ImGuiKey_Space;
    case VK_RETURN:
        return ImGuiKey_Enter;
    case VK_ESCAPE:
        return ImGuiKey_Escape;
    case VK_OEM_7:
        return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA:
        return ImGuiKey_Comma;
    case VK_OEM_MINUS:
        return ImGuiKey_Minus;
    case VK_OEM_PERIOD:
        return ImGuiKey_Period;
    case VK_OEM_2:
        return ImGuiKey_Slash;
    case VK_OEM_1:
        return ImGuiKey_Semicolon;
    case VK_OEM_PLUS:
        return ImGuiKey_Equal;
    case VK_OEM_4:
        return ImGuiKey_LeftBracket;
    case VK_OEM_5:
        return ImGuiKey_Backslash;
    case VK_OEM_6:
        return ImGuiKey_RightBracket;
    case VK_OEM_3:
        return ImGuiKey_GraveAccent;
    case VK_CAPITAL:
        return ImGuiKey_CapsLock;
    case VK_SCROLL:
        return ImGuiKey_ScrollLock;
    case VK_NUMLOCK:
        return ImGuiKey_NumLock;
    case VK_SNAPSHOT:
        return ImGuiKey_PrintScreen;
    case VK_PAUSE:
        return ImGuiKey_Pause;
    case VK_NUMPAD0:
        return ImGuiKey_Keypad0;
    case VK_NUMPAD1:
        return ImGuiKey_Keypad1;
    case VK_NUMPAD2:
        return ImGuiKey_Keypad2;
    case VK_NUMPAD3:
        return ImGuiKey_Keypad3;
    case VK_NUMPAD4:
        return ImGuiKey_Keypad4;
    case VK_NUMPAD5:
        return ImGuiKey_Keypad5;
    case VK_NUMPAD6:
        return ImGuiKey_Keypad6;
    case VK_NUMPAD7:
        return ImGuiKey_Keypad7;
    case VK_NUMPAD8:
        return ImGuiKey_Keypad8;
    case VK_NUMPAD9:
        return ImGuiKey_Keypad9;
    case VK_DECIMAL:
        return ImGuiKey_KeypadDecimal;
    case VK_DIVIDE:
        return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY:
        return ImGuiKey_KeypadMultiply;
    case VK_SUBTRACT:
        return ImGuiKey_KeypadSubtract;
    case VK_ADD:
        return ImGuiKey_KeypadAdd;
    case VK_LSHIFT:
        return ImGuiKey_LeftShift;
    case VK_LCONTROL:
        return ImGuiKey_LeftCtrl;
    case VK_LMENU:
        return ImGuiKey_LeftAlt;
    case VK_LWIN:
        return ImGuiKey_LeftSuper;
    case VK_RSHIFT:
        return ImGuiKey_RightShift;
    case VK_RCONTROL:
        return ImGuiKey_RightCtrl;
    case VK_RMENU:
        return ImGuiKey_RightAlt;
    case VK_RWIN:
        return ImGuiKey_RightSuper;
    case VK_APPS:
        return ImGuiKey_Menu;
    case '0':
        return ImGuiKey_0;
    case '1':
        return ImGuiKey_1;
    case '2':
        return ImGuiKey_2;
    case '3':
        return ImGuiKey_3;
    case '4':
        return ImGuiKey_4;
    case '5':
        return ImGuiKey_5;
    case '6':
        return ImGuiKey_6;
    case '7':
        return ImGuiKey_7;
    case '8':
        return ImGuiKey_8;
    case '9':
        return ImGuiKey_9;
    case 'A':
        return ImGuiKey_A;
    case 'B':
        return ImGuiKey_B;
    case 'C':
        return ImGuiKey_C;
    case 'D':
        return ImGuiKey_D;
    case 'E':
        return ImGuiKey_E;
    case 'F':
        return ImGuiKey_F;
    case 'G':
        return ImGuiKey_G;
    case 'H':
        return ImGuiKey_H;
    case 'I':
        return ImGuiKey_I;
    case 'J':
        return ImGuiKey_J;
    case 'K':
        return ImGuiKey_K;
    case 'L':
        return ImGuiKey_L;
    case 'M':
        return ImGuiKey_M;
    case 'N':
        return ImGuiKey_N;
    case 'O':
        return ImGuiKey_O;
    case 'P':
        return ImGuiKey_P;
    case 'Q':
        return ImGuiKey_Q;
    case 'R':
        return ImGuiKey_R;
    case 'S':
        return ImGuiKey_S;
    case 'T':
        return ImGuiKey_T;
    case 'U':
        return ImGuiKey_U;
    case 'V':
        return ImGuiKey_V;
    case 'W':
        return ImGuiKey_W;
    case 'X':
        return ImGuiKey_X;
    case 'Y':
        return ImGuiKey_Y;
    case 'Z':
        return ImGuiKey_Z;
    case VK_F1:
        return ImGuiKey_F1;
    case VK_F2:
        return ImGuiKey_F2;
    case VK_F3:
        return ImGuiKey_F3;
    case VK_F4:
        return ImGuiKey_F4;
    case VK_F5:
        return ImGuiKey_F5;
    case VK_F6:
        return ImGuiKey_F6;
    case VK_F7:
        return ImGuiKey_F7;
    case VK_F8:
        return ImGuiKey_F8;
    case VK_F9:
        return ImGuiKey_F9;
    case VK_F10:
        return ImGuiKey_F10;
    case VK_F11:
        return ImGuiKey_F11;
    case VK_F12:
        return ImGuiKey_F12;
    case VK_F13:
        return ImGuiKey_F13;
    case VK_F14:
        return ImGuiKey_F14;
    case VK_F15:
        return ImGuiKey_F15;
    case VK_F16:
        return ImGuiKey_F16;
    case VK_F17:
        return ImGuiKey_F17;
    case VK_F18:
        return ImGuiKey_F18;
    case VK_F19:
        return ImGuiKey_F19;
    case VK_F20:
        return ImGuiKey_F20;
    case VK_F21:
        return ImGuiKey_F21;
    case VK_F22:
        return ImGuiKey_F22;
    case VK_F23:
        return ImGuiKey_F23;
    case VK_F24:
        return ImGuiKey_F24;
    case VK_BROWSER_BACK:
        return ImGuiKey_AppBack;
    case VK_BROWSER_FORWARD:
        return ImGuiKey_AppForward;
    default:
        return ImGuiKey_None;
    }
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
                return;
            }

            if (lastKey == VK_BACK)
                lastKey = UnboundKey;

            configKey = lastKey;
            waitingForKey = false;
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

// Win32 message handler
LRESULT MenuCommon::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    // LOG_TRACE("msg: {:X}, wParam: {:X}, lParam: {:X}", msg, wParam, lParam);

    if (!State::Instance().isShuttingDown &&
        (msg == WM_QUIT || msg == WM_CLOSE ||
         msg == WM_DESTROY || /* classic messages but they are a bit late to capture */
         (msg == WM_SYSCOMMAND && wParam == SC_CLOSE /* window close*/)))
    {
        LOG_WARN("IsShuttingDown = true");
        State::Instance().isShuttingDown = true;
        return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);
    }

    if (State::Instance().isShuttingDown)
        return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);

    if (!_dx11Ready && !_dx12Ready && !_vulkanReady)
    {
        if (_isVisible)
        {
            LOG_INFO("No active features, closing ImGui");

            if (pfn_ClipCursor_hooked)
                pfn_ClipCursor(&_cursorLimit);

            _isVisible = false;
            _showMipmapCalcWindow = false;
            _showHudlessWindow = false;

            io.MouseDrawCursor = false;
            io.WantCaptureKeyboard = false;
            io.WantCaptureMouse = false;
        }

        return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);
    }

    bool rawRead = false;
    ImGuiKey imguiKey;
    RAWINPUT rawData {};
    UINT rawDataSize = sizeof(rawData);

    if (msg == WM_INPUT && GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &rawData, &rawDataSize,
                                           sizeof(rawData.data)) != (UINT) -1)
    {
        auto rawCode = GET_RAWINPUT_CODE_WPARAM(wParam);
        rawRead = true;
        receivingWmInputs = true;
        bool isKeyUp = (rawData.data.keyboard.Flags & RI_KEY_BREAK) != 0;
        if (isKeyUp && rawData.header.dwType == RIM_TYPEKEYBOARD && rawData.data.keyboard.VKey != 0)
        {
            lastKey = rawData.data.keyboard.VKey;

            if (!inputMenu)
            {
                inputMenu = rawData.data.keyboard.VKey == Config::Instance()->ShortcutKey.value_or_default();

                if (inputMenu)
                    LOG_DEBUG("Menu key pressed, will be switching menu");
            }

            if (!inputFps)
            {
                inputFps = rawData.data.keyboard.VKey == Config::Instance()->FpsShortcutKey.value_or_default();

                if (inputFps)
                    LOG_DEBUG("Menu key pressed, will be switching FPS");
            }

            if (!inputFG)
            {
                inputFG = rawData.data.keyboard.VKey == Config::Instance()->FGShortcutKey.value_or_default();

                if (inputFG)
                    LOG_DEBUG("Menu key pressed, will be switching FG mode");
            }

            if (!inputFpsCycle)
            {
                inputFpsCycle =
                    rawData.data.keyboard.VKey == Config::Instance()->FpsCycleShortcutKey.value_or_default();

                if (inputFpsCycle)
                    LOG_DEBUG("Menu key pressed, will be switching FPS mode");
            }
        }
    }

    if (!lastKey && msg == WM_KEYUP)
        lastKey = static_cast<int>(wParam);

    if (!inputMenu)
    {
        inputMenu = msg == WM_KEYUP && wParam == Config::Instance()->ShortcutKey.value_or_default();

        if (inputMenu)
            LOG_DEBUG("Menu key pressed, will be switching menu");
    }

    if (!inputFps)
    {
        inputFps = msg == WM_KEYUP && wParam == Config::Instance()->FpsShortcutKey.value_or_default();

        if (inputFps)
            LOG_DEBUG("Menu key pressed, will be switching FPS");
    }

    if (!inputFG)
    {
        inputFG = msg == WM_KEYUP && wParam == Config::Instance()->FGShortcutKey.value_or_default();

        if (inputFG)
            LOG_DEBUG("Menu key pressed, will be switching FG mode");
    }

    if (!inputFpsCycle)
    {
        inputFpsCycle = msg == WM_KEYUP && wParam == Config::Instance()->FpsCycleShortcutKey.value_or_default();

        if (inputFpsCycle)
            LOG_DEBUG("Menu key pressed, will be switching FPS mode");
    }

    // SHIFT + DEL - Debug dump
    if (msg == WM_KEYUP && wParam == VK_DELETE && (GetKeyState(VK_SHIFT) & 0x8000))
    {
        State::Instance().xessDebug = true;
        return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);
    }

    // ImGui
    if (_isVisible)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        {

            if (msg == WM_KEYUP || msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP ||
                msg == WM_SYSKEYUP ||
                (msg == WM_INPUT && rawRead && rawData.header.dwType == RIM_TYPEMOUSE &&
                 (rawData.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP ||
                  rawData.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP ||
                  rawData.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)))
            {
                LOG_TRACE("ImGui handled & called original, hWnd:{0:X} msg:{1:X} wParam:{2:X} lParam:{3:X}",
                          (ULONG64) hWnd, msg, (ULONG64) wParam, (ULONG64) lParam);
                return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);
            }
            else
            {
                LOG_TRACE("ImGui handled, hWnd:{0:X} msg:{1:X} wParam:{2:X} lParam:{3:X}", (ULONG64) hWnd, msg,
                          (ULONG64) wParam, (ULONG64) lParam);
                return TRUE;
            }
        }

        switch (msg)
        {
        case WM_KEYUP:
            if (wParam != Config::Instance()->ShortcutKey.value_or_default())
                return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);

            imguiKey = ImGui_ImplWin32_VirtualKeyToImGuiKey(wParam);
            io.AddKeyEvent(imguiKey, false);

            break;

        case WM_LBUTTONDOWN:
            io.AddMouseButtonEvent(0, true);
            return TRUE;

        case WM_LBUTTONUP:
            io.AddMouseButtonEvent(0, false);
            break;

        case WM_RBUTTONDOWN:
            io.AddMouseButtonEvent(1, true);
            return TRUE;

        case WM_RBUTTONUP:
            io.AddMouseButtonEvent(1, false);
            break;

        case WM_MBUTTONDOWN:
            io.AddMouseButtonEvent(2, true);
            return TRUE;

        case WM_MBUTTONUP:
            io.AddMouseButtonEvent(2, false);
            break;

        case WM_LBUTTONDBLCLK:
            io.AddMouseButtonEvent(0, true);
            return TRUE;

        case WM_RBUTTONDBLCLK:
            io.AddMouseButtonEvent(1, true);
            return TRUE;

        case WM_MBUTTONDBLCLK:
            io.AddMouseButtonEvent(2, true);
            return TRUE;

        case WM_KEYDOWN:
            imguiKey = ImGui_ImplWin32_VirtualKeyToImGuiKey(wParam);
            io.AddKeyEvent(imguiKey, true);
            return TRUE;

        case WM_SYSKEYUP:
            break;

        case WM_SYSKEYDOWN:
        case WM_MOUSEMOVE:
        case WM_SETCURSOR:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
            LOG_TRACE("switch handled, hWnd:{0:X} msg:{1:X} wParam:{2:X} lParam:{3:X}", (ULONG64) hWnd, msg,
                      (ULONG64) wParam, (ULONG64) lParam);
            return TRUE;

        case WM_INPUT:
            if (!rawRead)
                return TRUE;

            if (rawData.header.dwType == RIM_TYPEMOUSE)
            {
                if (rawData.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
                {
                    io.AddMouseButtonEvent(0, true);
                }
                else if (rawData.data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
                {
                    io.AddMouseButtonEvent(0, false);
                    break;
                }
                if (rawData.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
                {
                    io.AddMouseButtonEvent(1, true);
                }
                else if (rawData.data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
                {
                    io.AddMouseButtonEvent(1, false);
                    break;
                }
                if (rawData.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
                {
                    io.AddMouseButtonEvent(2, true);
                }
                else if (rawData.data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
                {
                    io.AddMouseButtonEvent(2, false);
                    break;
                }

                if (rawData.data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
                    io.AddMouseWheelEvent(0, static_cast<short>(rawData.data.mouse.usButtonData) / (float) WHEEL_DELTA);
            }
            else
            {
                LOG_TRACE("WM_INPUT hWnd:{0:X} msg:{1:X} wParam:{2:X} lParam:{3:X}", (ULONG64) hWnd, msg,
                          (ULONG64) wParam, (ULONG64) lParam);
            }

            return TRUE;

        default:
            break;
        }
    }

    return CallWindowProc(_oWndProc, hWnd, msg, wParam, lParam);
}

void KeyUp(UINT vKey)
{
    inputMenu = vKey == Config::Instance()->ShortcutKey.value_or_default();
    inputFps = vKey == Config::Instance()->FpsShortcutKey.value_or_default();
    inputFG = vKey == Config::Instance()->FGShortcutKey.value_or_default();
    inputFpsCycle = vKey == Config::Instance()->FpsCycleShortcutKey.value_or_default();
}

std::string MenuCommon::GetBackendName(std::string* code)
{
    if (*code == "fsr21")
        return "FSR 2.1.2";

    if (*code == "fsr22")
        return "FSR 2.2.1";

    if (*code == "fsr31")
        return "FSR 3.X";

    if (*code == "fsr21_12")
        return "FSR 2.1.2 w/Dx12";

    if (*code == "fsr22_12")
        return "FSR 2.2.1 w/Dx12";

    if (*code == "fsr31_12")
        return "FSR 3.X w/Dx12";

    if (*code == "xess")
        return "XeSS";

    if (*code == "xess_12")
        return "XeSS w/Dx12";

    if (*code == "dlss")
        return "DLSS";

    if (*code == "dlssd")
        return "DLSSD";

    return "????";
}

std::string MenuCommon::GetBackendCode(const API api)
{
    if (State::Instance().currentFeature != nullptr && State::Instance().currentFeature->Name() == "DLSSD")
        return "dlssd";

    std::string code;

    if (api == DX11)
        code = Config::Instance()->Dx11Upscaler.value_or_default();
    else if (api == DX12)
        code = Config::Instance()->Dx12Upscaler.value_or_default();
    else
        code = Config::Instance()->VulkanUpscaler.value_or_default();

    return code;
}

void MenuCommon::GetCurrentBackendInfo(const API api, std::string* code, std::string* name)
{
    *code = GetBackendCode(api);
    *name = GetBackendName(code);
}

void MenuCommon::AddDx11Backends(std::string* code, std::string* name)
{
    std::string selectedUpscalerName = "";
    bool fsr4Possible =
        Config::Instance()->Fsr4Update.value_or_default() || State::Instance().isRunningOnRDNA4.value_or(false) ||
        (Config::Instance()->Fsr4ForceEnableInt8.value_or_default() && FfxApiProxy::Dx12Module_SR() != nullptr &&
         FfxApiProxy::VersionDx12_SR() >= feature_version { 4, 1, 1 });

    std::string fsr3xName = fsr4Possible ? "FSR 3.X/4 w/Dx12" : "FSR 3.X w/Dx12";

    if (State::Instance().newBackend == "fsr22" || (State::Instance().newBackend == "" && *code == "fsr22"))
        selectedUpscalerName = "FSR 2.2.1";
    else if (State::Instance().newBackend == "fsr22_12" || (State::Instance().newBackend == "" && *code == "fsr22_12"))
        selectedUpscalerName = "FSR 2.2.1 w/Dx12";
    else if (State::Instance().newBackend == "fsr21_12" || (State::Instance().newBackend == "" && *code == "fsr21_12"))
        selectedUpscalerName = "FSR 2.1.2 w/Dx12";
    else if (State::Instance().newBackend == "fsr31" || (State::Instance().newBackend == "" && *code == "fsr31"))
        selectedUpscalerName = "FSR 3.X";
    else if (State::Instance().newBackend == "fsr31_12" || (State::Instance().newBackend == "" && *code == "fsr31_12"))
        selectedUpscalerName = fsr3xName;
    else if (Config::Instance()->DLSSEnabled.value_or_default() &&
             (State::Instance().newBackend == "dlss" || (State::Instance().newBackend == "" && *code == "dlss")))
        selectedUpscalerName = "DLSS";
    else if (State::Instance().newBackend == "xess" || (State::Instance().newBackend == "" && *code == "xess"))
        selectedUpscalerName = "XeSS";
    else
        selectedUpscalerName = "XeSS w/Dx12";

    if (ImGui::BeginCombo("", selectedUpscalerName.c_str()))
    {
        if (ImGui::Selectable("XeSS", *code == "xess"))
            State::Instance().newBackend = "xess";

        if (ImGui::Selectable("FSR 2.2.1", *code == "fsr22"))
            State::Instance().newBackend = "fsr22";

        if (ImGui::Selectable("FSR 3.X", *code == "fsr31"))
            State::Instance().newBackend = "fsr31";

        if (ImGui::Selectable("XeSS w/Dx12", *code == "xess_12"))
            State::Instance().newBackend = "xess_12";

        if (ImGui::Selectable("FSR 2.1.2 w/Dx12", *code == "fsr21_12"))
            State::Instance().newBackend = "fsr21_12";

        if (ImGui::Selectable("FSR 2.2.1 w/Dx12", *code == "fsr22_12"))
            State::Instance().newBackend = "fsr22_12";

        if (ImGui::Selectable(fsr3xName.c_str(), *code == "fsr31_12"))
            State::Instance().newBackend = "fsr31_12";

        if (Config::Instance()->DLSSEnabled.value_or_default() && ImGui::Selectable("DLSS", *code == "dlss"))
            State::Instance().newBackend = "dlss";

        ImGui::EndCombo();
    }
}

void MenuCommon::AddDx12Backends(std::string* code, std::string* name)
{
    std::string selectedUpscalerName = "";
    bool fsr4Possible =
        Config::Instance()->Fsr4Update.value_or_default() || State::Instance().isRunningOnRDNA4.value_or(false) ||
        (Config::Instance()->Fsr4ForceEnableInt8.value_or_default() && FfxApiProxy::Dx12Module_SR() != nullptr &&
         FfxApiProxy::VersionDx12_SR() >= feature_version { 4, 1, 1 });

    std::string fsr3xName = fsr4Possible ? "FSR 3.X/4" : "FSR 3.X";

    if (State::Instance().newBackend == "fsr21" || (State::Instance().newBackend == "" && *code == "fsr21"))
        selectedUpscalerName = "FSR 2.1.2";
    else if (State::Instance().newBackend == "fsr22" || (State::Instance().newBackend == "" && *code == "fsr22"))
        selectedUpscalerName = "FSR 2.2.1";
    else if (State::Instance().newBackend == "fsr31" || (State::Instance().newBackend == "" && *code == "fsr31"))
        selectedUpscalerName = fsr3xName;
    else if (Config::Instance()->DLSSEnabled.value_or_default() &&
             (State::Instance().newBackend == "dlss" || (State::Instance().newBackend == "" && *code == "dlss")))
        selectedUpscalerName = "DLSS";
    else
        selectedUpscalerName = "XeSS";

    if (ImGui::BeginCombo("", selectedUpscalerName.c_str()))
    {
        if (ImGui::Selectable("XeSS", *code == "xess"))
            State::Instance().newBackend = "xess";

        if (ImGui::Selectable("FSR 2.1.2", *code == "fsr21"))
            State::Instance().newBackend = "fsr21";

        if (ImGui::Selectable("FSR 2.2.1", *code == "fsr22"))
            State::Instance().newBackend = "fsr22";

        if (ImGui::Selectable(fsr3xName.c_str(), *code == "fsr31"))
            State::Instance().newBackend = "fsr31";

        if (Config::Instance()->DLSSEnabled.value_or_default() && ImGui::Selectable("DLSS", *code == "dlss"))
            State::Instance().newBackend = "dlss";

        ImGui::EndCombo();
    }
}

void MenuCommon::AddVulkanBackends(std::string* code, std::string* name)
{
    std::string selectedUpscalerName = "";
    bool fsr4Possible =
        Config::Instance()->Fsr4Update.value_or_default() || State::Instance().isRunningOnRDNA4.value_or(false) ||
        (Config::Instance()->Fsr4ForceEnableInt8.value_or_default() && FfxApiProxy::Dx12Module_SR() != nullptr &&
         FfxApiProxy::VersionDx12_SR() >= feature_version { 4, 1, 1 });

    std::string fsr3xName = fsr4Possible ? "FSR 3.X/4 w/Dx12" : "FSR 3.X w/Dx12";

    if (State::Instance().newBackend == "fsr21" || (State::Instance().newBackend == "" && *code == "fsr21"))
        selectedUpscalerName = "FSR 2.1.2";
    else if (State::Instance().newBackend == "fsr31" || (State::Instance().newBackend == "" && *code == "fsr31"))
        selectedUpscalerName = "FSR 3.X";
    else if (State::Instance().newBackend == "xess" || (State::Instance().newBackend == "" && *code == "xess"))
        selectedUpscalerName = "XeSS";
    else if (Config::Instance()->DLSSEnabled.value_or_default() &&
             (State::Instance().newBackend == "dlss" || (State::Instance().newBackend == "" && *code == "dlss")))
        selectedUpscalerName = "DLSS";
    else if (State::Instance().newBackend == "fsr31_12" || (State::Instance().newBackend == "" && *code == "fsr31_12"))
        selectedUpscalerName = fsr3xName;
    else if (State::Instance().newBackend == "fsr21_12" || (State::Instance().newBackend == "" && *code == "fsr21_12"))
        selectedUpscalerName = "FSR 2.1.2 w/Dx12";
    else
        selectedUpscalerName = "FSR 2.2.1";

    if (ImGui::BeginCombo("", selectedUpscalerName.c_str()))
    {
        if (ImGui::Selectable("XeSS", *code == "xess"))
            State::Instance().newBackend = "xess";

        if (ImGui::Selectable("FSR 2.1.2", *code == "fsr21"))
            State::Instance().newBackend = "fsr21";

        if (ImGui::Selectable("FSR 2.2.1", *code == "fsr22"))
            State::Instance().newBackend = "fsr22";

        if (ImGui::Selectable("FSR 3.X", *code == "fsr31"))
            State::Instance().newBackend = "fsr31";

        if (Config::Instance()->DLSSEnabled.value_or_default() && ImGui::Selectable("DLSS", *code == "dlss"))
            State::Instance().newBackend = "dlss";

        if (ImGui::Selectable("FSR 2.1.2 w/Dx12", *code == "fsr21_12"))
            State::Instance().newBackend = "fsr21_12";

        if (ImGui::Selectable("FSR 3.X w/Dx12", *code == "fsr31_12"))
            State::Instance().newBackend = "fsr31_12";

        ImGui::EndCombo();
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
            LOG_DEBUG("DLSS Presets overridden by Opti, using Opti preset indices with ratio: {}", ratio);

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
            LOG_DEBUG("DLSS Presets overridden externally, using external preset index: {}",
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

constexpr uint32_t NV_PRESET_LATEST = 0x00FFFFFF;

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

static ImVec4 toneMapColor(const ImVec4& color)
{
    if (State::Instance().isHdrActive ||
        (!Config::Instance()->OverlayMenu.value_or_default() && State::Instance().currentFeature != nullptr &&
         State::Instance().currentFeature->IsHdr()))
    {
        // Controls how strongly HDR/UI colors are pushed into the tone mapper before compression.
        // Higher values make colors brighter before mapping; lower values make the result dimmer.
        constexpr float exposure = 1.0f;

        // Blends between original color and fully tone-mapped color.
        // 0.0 = no tone mapping, 1.0 = full Reinhard compression.
        constexpr float strength = 1.0f;

        float peak = std::max(color.x, std::max(color.y, color.z));

        if (peak <= 0.0f)
            return color;

        float exposedPeak = peak * exposure;
        float mappedPeak = exposedPeak / (1.0f + exposedPeak);

        float reinhardScale = mappedPeak / peak;
        float scale = 1.0f + (reinhardScale - 1.0f) * strength;

        return ImVec4(color.x * scale, color.y * scale, color.z * scale, color.w);
    }

    return color;
}

static void MenuHdrCheck(ImGuiIO io)
{
    // If game is using HDR, apply tone mapping to the ImGui style
    if (State::Instance().isHdrActive ||
        (!Config::Instance()->OverlayMenu.value_or_default() && State::Instance().currentFeature != nullptr &&
         State::Instance().currentFeature->IsHdr()))
    {
        if (!_hdrTonemapApplied)
        {
            ImGuiStyle& style = ImGui::GetStyle();

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
    else
    {
        if (_hdrTonemapApplied)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            CopyMemory(style.Colors, SdrColors, sizeof(style.Colors));
            _hdrTonemapApplied = false;
        }
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
    default:
        return std::format("{}", source);
    }
}

inline static std::string GetDispatchString(UINT source)
{
    switch (source)
    {
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

static void ApplyThemeStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    auto conf = Config::Instance();
    bool lightTheme = conf->LightTheme.value_or_default();

    style.WindowRounding = 2.0f;
    style.ChildRounding = 1.0f;
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

    c[ImGuiCol_Tab] = AccentSoft();
    c[ImGuiCol_TabHovered] = AccentMed();
    c[ImGuiCol_TabSelected] = AccentSoft();
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

    if (inputManual && _handle != nullptr)
        UpdateManualInput(_handle);
}

bool MenuCommon::RenderMenu()
{
    if (!_isInited)
        return false;

    if (!pfn_SetCursorPos_hooked)
        AttachHooks();

    auto& state = State::Instance();
    auto config = Config::Instance();

    auto now = Util::MillisecondsNow();
    double frameTime = 0.0;
    double frameRate = 0.0;

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

        if (inputManual && _handle != nullptr)
            UpdateManualInput(_handle);
    }
    else
    {
        frameTime = lastFrameTime;
        frameRate = 1000.0 / frameTime;
    }

    state.frameTimes.pop_front();
    state.frameTimes.push_back(frameTime);

    ImGuiIO& io = ImGui::GetIO();
    (void) io;
    auto currentFeature = state.currentFeature;

    bool newFrame = false;

    // Moved here to prevent gamepad key replay
    if (_isVisible)
    {
        if (hasGamepad)
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    }
    else
    {
        hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
        io.BackendFlags &= 30;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;
    }

    // Handle Inputs
    {
        if (inputFG)
        {
            inputFG = false;

            if (state.activeFgInput != FGInput::NoFG && state.activeFgOutput != FGOutput::NoFG &&
                (state.currentFGSwapchain != nullptr || state.activeFgInput == FGInput::Nukems))
            {
                config->FGEnabled = !config->FGEnabled.value_or_default();
                LOG_DEBUG("FG toggle key pressed, setting FGEnabled to {}", config->FGEnabled.value_or_default());

                if (config->FGEnabled.value_or_default())
                    state.FGchanged = true;
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

            LOG_DEBUG("Menu key pressed, {0}", _isVisible ? "opening ImGui" : "closing ImGui");

            if (_isVisible)
            {
                ApplyThemeStyle();

                refreshRate = Util::GetActiveRefreshRate(_handle);
                config->ReloadFakenvapi();
                auto dllPath = std::filesystem::path(config->MainDllPath.value()) / L"dlssg_to_fsr3_amd_is_better.dll";
                state.NukemsFilesAvailable = gExists.Get(dllPath);

                if (State::Instance().currentFeature != nullptr)
                {
                    if (State::Instance().currentFeature->Name() == "DLSSD")
                        comboPreset = config->DLSSDRenderPresetForAll.value_or_default();
                    else if (State::Instance().currentFeature->Name() == "DLSS")
                        comboPreset = config->RenderPresetForAll.value_or_default();
                }

                if (pfn_ClipCursor_hooked)
                {
                    _ssRatio = 0;

                    if (GetClipCursor(&_cursorLimit))
                        pfn_ClipCursor(nullptr);

                    GetCursorPos(&_lastPoint);
                }
            }
            else
            {
                ImGui::CloseCurrentPopup();

                if (pfn_ClipCursor_hooked)
                    pfn_ClipCursor(&_cursorLimit);

                _showMipmapCalcWindow = false;
                _showHudlessWindow = false;
            }

            io.MouseDrawCursor = _isVisible;
            io.WantCaptureKeyboard = _isVisible;
            io.WantCaptureMouse = _isVisible;
        }

        inputFpsCycle = false;
    }

    // Version check
    bool frameTimesCalculated = false;
    const double splashTime = 7000.0;
    const double fadeTime = 1000.0;
    const double updateNoticeTime = 7000.0;
    const double updateNoticeFade = 1000.0;
    static std::string splashMessage;

    struct VersionCheckStatus
    {
        bool completed = false;
        bool updateAvailable = false;
        std::string latestTag;
        std::string latestUrl;
        std::string error;
    } versionStatus;

    {
        std::scoped_lock lock(state.versionCheckMutex);
        versionStatus.completed = state.versionCheckCompleted;
        versionStatus.updateAvailable = state.updateAvailable;
        versionStatus.latestTag = state.latestVersionTag;
        versionStatus.latestUrl = state.latestVersionUrl;
        versionStatus.error = state.versionCheckError;
    }

    const auto& currentVersionText = VersionCheck::CurrentVersionString();

    if (versionStatus.completed && versionStatus.updateAvailable && !versionStatus.latestTag.empty())
    {
        if (updateNoticeTag != versionStatus.latestTag)
        {
            updateNoticeTag = versionStatus.latestTag;
            updateNoticeUrl = versionStatus.latestUrl;
            updateNoticeStart = now;
            updateNoticeLimit = updateNoticeStart + updateNoticeTime;
            updateNoticeVisible = true;
        }
    }

    if (splashLimit < 1.0f)
    {
        splashStart = now + 100.0;
        splashLimit = splashStart + splashTime;

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        splashMessage = splashText[std::rand() % splashText.size()];
    }

    // New frame check
    if ((!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit) ||
        (updateNoticeVisible && now < updateNoticeLimit) || config->ShowFps.value_or_default() || _isVisible)
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

        MenuHdrCheck(io);
        ImGui::NewFrame();

        newFrame = true;
    }

    float menuResScale = MenuResolutionScale(io);

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
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

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

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
        }
    }

    if (updateNoticeVisible)
    {
        if (now >= updateNoticeLimit)
        {
            updateNoticeVisible = false;
        }
        else
        {
            ImGui::SetNextWindowSize({ 0.0f, 0.0f });
            ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default());
            ImGui::SetNextWindowPos(updateNoticePosition, ImGuiCond_Always);

            float windowAlpha = 1.0f;
            if (auto diff = now - updateNoticeStart; diff < updateNoticeFade)
                windowAlpha = static_cast<float>(diff / updateNoticeFade);
            else if (auto diff = updateNoticeLimit - now; diff < updateNoticeFade)
                windowAlpha = static_cast<float>(diff / updateNoticeFade);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

            bool pushedFont = false;
            if (ImGui::Begin("有可用更新", nullptr,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav))
            {
                float splashScale = 1.0f;
                float baseScaleHeight = 720.0f;

                if (io.DisplaySize.y > baseScaleHeight)
                    splashScale = io.DisplaySize.y / baseScaleHeight;

                if (config->UseHQFont.value_or_default())
                {
                    ImGui::PushFontSize(std::round(splashScale * fontSize));
                    pushedFont = true;
                }
                else
                {
                    ImGui::SetWindowFontScale(splashScale);
                }

                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.0f, 0.0f, 1.0f)), "发现 OptiScaler 更新");
                ImGui::Spacing();
                ImGui::Text("按 %s 查看更多信息",
                            Keybind::KeyNameFromVirtualKeyCode(config->ShortcutKey.value_or_default()).c_str());

                if (pushedFont)
                    ImGui::PopFontSize();
            }

            updateNoticeSize = ImGui::GetWindowSize();
            ImGui::End();

            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);

            updateNoticePosition.x = 0.0f;
            float baseY = io.DisplaySize.y - updateNoticeSize.y;

            if (!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit)
                baseY = splashPosition.y - updateNoticeSize.y - 10.0f;

            if (baseY < 0.0f)
                baseY = 0.0f;

            updateNoticePosition.y = baseY;
        }
    }

    // FPS Overlay font
    auto fpsScale = config->FpsScale.value_or(menuResScale);

    // Update frame time & upscaler time averages
    float averageFrameTime = 0.0f;
    float averageUpscalerFT = 0.0f;

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

        float lastFT = static_cast<float>(state.frameTimes.empty() ? 0.0f : state.frameTimes.back());
        float lastUT = static_cast<float>(state.upscaleTimes.empty() ? 0.0f : state.upscaleTimes.back());
        gFrameTimes.Push(lastFT);
        gUpscalerTimes.Push(lastUT);

        averageFrameTime = gFrameTimes.Average();
        averageUpscalerFT = gUpscalerTimes.Average();
    }

    // If Fps overlay is visible
    if (config->ShowFps.value_or_default())
    {
        bool stylePushed = false;

        static auto defaultStyle = ImGuiStyle();

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

        // Set overlay position
        ImGui::SetNextWindowPos(overlayPosition, ImGuiCond_Always);

        // Set overlay window properties
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));            // Transparent border
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));           // Transparent frame background
        ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default()); // Transparent background

        ImVec4 green(0.0f, 1.0f, 0.0f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotLines, toneMapColor(green));

        if (ImGui::Begin("性能覆盖层", nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav))
        {
            std::string api;
            if (state.isRunningOnDXVK || state.isRunningOnLinux)
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

            if (config->UseHQFont.value_or_default())
                ImGui::PushFontSize(std::round(fpsScale * fontSize));
            else
                ImGui::SetWindowFontScale(fpsScale);

            std::string firstLine = "";
            std::string secondLine = "";
            std::string thirdLine = "";

            auto fg = state.currentFG;
            auto fgText = (fg != nullptr && fg->IsActive() && !fg->IsPaused()) ? ("(" + std::string(fg->Name()) + ")")
                                                                               : std::string();

            // Prepare Line 1
            if (config->FpsOverlayType.value_or_default() == FpsOverlay_JustFPS)
            {
                if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                {
                    firstLine = StrFmt("%s | FPS: %6.1f/%5.1f %s", api.c_str(), frameRate,
                                       frameRate / (float) (fg->GetInterpolatedFrameCount() + 1), fgText.c_str());
                }
                else
                {
                    firstLine = StrFmt("%s | FPS: %6.1f %s", api.c_str(), frameRate, fgText.c_str());
                }
            }
            else if (config->FpsOverlayType.value_or_default() == FpsOverlay_Simple)
            {
                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                    {
                        firstLine = StrFmt("%s | FPS: %6.1f/%5.1f, %7.2f ms %s | %s -> %s %u.%u.%u", api.c_str(),
                                           frameRate, frameRate / (float) (fg->GetInterpolatedFrameCount() + 1),
                                           frameTime, fgText.c_str(), state.currentInputApiName.c_str(),
                                           currentFeature->Name().c_str(), currentFeature->Version().major,
                                           currentFeature->Version().minor, currentFeature->Version().patch);
                    }
                    else
                    {
                        firstLine = StrFmt("%s | FPS: %6.1f, %7.2f ms %s | %s -> %s %u.%u.%u", api.c_str(), frameRate,
                                           frameTime, fgText.c_str(), state.currentInputApiName.c_str(),
                                           currentFeature->Name().c_str(), currentFeature->Version().major,
                                           currentFeature->Version().minor, currentFeature->Version().patch);
                    }
                }
                else
                {
                    if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                    {
                        firstLine = StrFmt("%s | FPS: %6.1f/%5.1f, %7.2f ms %s", api.c_str(), frameRate,
                                           frameRate / (float) (fg->GetInterpolatedFrameCount() + 1), frameTime,
                                           fgText.c_str());
                    }
                    else
                    {
                        firstLine =
                            StrFmt("%s | FPS: %6.1f, %7.2f ms %s", api.c_str(), frameRate, frameTime, fgText.c_str());
                    }
                }
            }
            else
            {
                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                    {
                        firstLine =
                            StrFmt("%s | FPS: %6.1f/%5.1f, Avg: %6.1f %s | %s -> %s %u.%u.%u", api.c_str(), frameRate,
                                   frameRate / (float) (fg->GetInterpolatedFrameCount() + 1),
                                   1000.0f / averageFrameTime, fgText.c_str(), state.currentInputApiName.c_str(),
                                   currentFeature->Name().c_str(), currentFeature->Version().major,
                                   currentFeature->Version().minor, currentFeature->Version().patch);
                    }
                    else
                    {
                        firstLine =
                            StrFmt("%s | FPS: %6.1f, Avg: %6.1f %s | %s -> %s %u.%u.%u", api.c_str(), frameRate,
                                   1000.0f / averageFrameTime, fgText.c_str(), state.currentInputApiName.c_str(),
                                   currentFeature->Name().c_str(), currentFeature->Version().major,
                                   currentFeature->Version().minor, currentFeature->Version().patch);
                    }
                }
                else
                {
                    if (fg != nullptr && fg->IsActive() && !fg->IsPaused())
                    {
                        firstLine = StrFmt("%s | FPS: %6.1f/%5.1f, Avg: %6.1f %s", api.c_str(), frameRate,
                                           frameRate / (float) (fg->GetInterpolatedFrameCount() + 1),
                                           1000.0f / averageFrameTime, fgText.c_str());
                    }
                    else
                    {
                        firstLine = StrFmt("%s | FPS: %6.1f, Avg: %6.1f %s", api.c_str(), frameRate,
                                           1000.0f / averageFrameTime, fgText.c_str());
                    }
                }
            }

            // Prepare Line 2
            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Detailed)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                secondLine = StrFmt("帧时间：%7.2f ms，平均：%7.2f ms", state.frameTimes.back(), averageFrameTime);
            }

            // Prepare Line 3
            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Full)
            {
                thirdLine =
                    StrFmt("超分器时间：%7.2f ms，平均：%7.2f ms", state.upscaleTimes.back(), averageUpscalerFT);
            }

            ImVec2 plotSize;
            if (config->FpsOverlayHorizontal.value_or_default())
            {
                plotSize = { fpsScale * 150, fpsScale * 16 };
            }
            else
            {
                // Find the widest text width
                auto firstSize = ImGui::CalcTextSize(firstLine.c_str());
                auto secondSize = ImGui::CalcTextSize(secondLine.c_str());
                auto thirdSize = ImGui::CalcTextSize(thirdLine.c_str());
                auto textWidth = 0.0f;

                if (firstSize.x > secondSize.x)
                    textWidth = firstSize.x > thirdSize.x ? firstSize.x : thirdSize.x;
                else
                    textWidth = secondSize.x > thirdSize.x ? secondSize.x : thirdSize.x;

                auto minWidth = fpsScale * 300.0f;
                auto plotWidth = textWidth < minWidth ? minWidth : textWidth;

                plotSize = { plotWidth, fpsScale * 30 };
            }

            // Draw the overlay
            ImGui::Text(firstLine.c_str());

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Detailed)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                ImGui::Text(secondLine.c_str());
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_DetailedGraph)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                    ImGui::SameLine(0.0f, 0.0f);

                // Graph of frame times
                ImGui::PlotLines(
                    "##FrameTimeGraph",
                    [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
                    &gFrameTimes, plotWidth, 0, nullptr, 0.0f, 66.6f, plotSize);
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_Full)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text(" | ");
                    ImGui::SameLine(0.0f, 0.0f);
                }
                else
                {
                    ImGui::Spacing();
                }

                ImGui::Text(thirdLine.c_str());
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_FullGraph)
            {
                if (config->FpsOverlayHorizontal.value_or_default())
                    ImGui::SameLine(0.0f, 0.0f);

                // Graph of upscaler times
                ImGui::PlotLines(
                    "##UpscalerFrameTimeGraph",
                    [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
                    &gUpscalerTimes, plotWidth, 0, nullptr, 0.0f, 20.0f, plotSize);
            }

            if (config->FpsOverlayType.value_or_default() >= FpsOverlay_ReflexTimings)
            {
                constexpr auto delayBetweenPollsMs = 500;
                static auto previousPoll = 0.0;
                static bool gotData = false;
                if (previousPoll <= 0.001 || previousPoll + delayBetweenPollsMs < now)
                {
                    gotData = ReflexHooks::updateTimingData();
                    previousPoll = now;
                }

                auto& timingData = ReflexHooks::timingData;

                if (gotData && timingData[TimingType::TimeRange].has_value())
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    constexpr float offsetForText = 155;

                    const auto& rangeInNs = timingData[TimingType::TimeRange].value().length;

                    UINT64 localFrameCount = 0;

                    if (fg != nullptr)
                        localFrameCount = fg->FrameCount();

                    ImGui::Text("FGId: %llu, RfxId: %llu", localFrameCount, state.reflexFrameId);
                    ImGui::Text("Reflex 时序，整帧：%.1fms", rangeInNs / 1000.0);

                    const auto maxWidth =
                        config->FpsOverlayHorizontal.value_or_default() ? ImGui::GetWindowWidth() : plotSize.x;

                    const auto drawTiming = [&](TimingType type, const char* desc, ImVec4 color)
                    {
                        if (!timingData[type].has_value())
                            return;

                        auto toneMappedColor = toneMapColor(color);

                        auto& timing = timingData[type].value();
                        float duration = static_cast<float>(timing.length * rangeInNs / 1000.0);
                        ImGui::TextColored(toneMappedColor, "%-12s %4.1fms", desc, duration);
                        auto leftLimit = ImGui::GetItemRectMin().x + offsetForText * fpsScale;
                        auto start = static_cast<float>(leftLimit + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                        timing.position);
                        auto end = static_cast<float>(start + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                  timing.length);
                        auto pos = ImVec2(start, ImGui::GetItemRectMin().y);
                        auto size = ImVec2(end, ImGui::GetItemRectMax().y);
                        drawList->AddRectFilled(pos, size, ImGui::ColorConvertFloat4ToU32(toneMappedColor));
                    };

                    drawTiming(TimingType::Simulation, "模拟", ImVec4(0.768f, 0.169f, 0.169f, 1.0f));
                    drawTiming(TimingType::RenderSubmit, "渲染提交", ImVec4(0.235f, 0.705f, 0.294f, 1.0f));
                    drawTiming(TimingType::Present, "呈现", ImVec4(1.0f, 0.88f, 0.098f, 1.0f));
                    drawTiming(TimingType::Driver, "驱动", ImVec4(0.263f, 0.388f, 0.847f, 1.0f));
                    drawTiming(TimingType::OsRenderQueue, "渲染队列", ImVec4(0.76f, 0.51f, 0.188f, 1.0f));
                    drawTiming(TimingType::GpuRender, "GPU 渲染", ImVec4(0.569f, 0.117f, 0.705f, 1.0f));
                }
            }
        }

        ImGui::PopStyleColor(5); // Restore the style

        // Get size for postioning
        overlaySize = ImGui::GetWindowSize();

        if (config->UseHQFont.value_or_default())
            ImGui::PopFontSize();

        ImGui::End();

        if (stylePushed)
            ImGui::PopStyleVar(7);

        // Left / Right
        if (config->FpsOverlayPos.value_or_default() == 0 || config->FpsOverlayPos.value_or_default() == 2)
            overlayPosition.x = 0;
        else
            overlayPosition.x = io.DisplaySize.x - overlaySize.x;

        // Top / Bottom
        if (config->FpsOverlayPos.value_or_default() < 2)
        {
            overlayPosition.y = 0;
        }
        else
        {
            // Prevent overlapping with splash message
            if (!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit)
                overlayPosition.y = io.DisplaySize.y - overlaySize.y - splashSize.y;
            else
                overlayPosition.y = io.DisplaySize.y - overlaySize.y;
        }
    }

    if (_isVisible)
    {
        // Check for gpu support
        {
            // DXVK might call vulkan device creation which would destroy our objects
            ScopedSkipVulkanHooks skipVulkanHooks;
            CheckForGPU();
        }

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

        if (lastMenuScale != menuResScale)
        {
            lastMenuScale = menuResScale;

            // if UI scale is changed rescale the style
            ImGuiStyle& style = ImGui::GetStyle();
            ImGuiStyle styleold = style; // Backup colors
            style = ImGuiStyle();        // IMPORTANT: ScaleAllSizes will change the original size,
                                         // so we should reset all style config

            ApplyThemeStyle();

            style.ScaleAllSizes(menuResScale);
            style.MouseCursorScale = 1.0f;
            CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors

            ImGui::SetNextWindowSize({ 1.0f, 1.0f });
        }

        // Main menu window
        if (windowTitle.empty())
        {
            windowTitle =
                StrFmt("%s - %s %s %s %s", VER_PRODUCT_NAME, state.GameExe.c_str(),
                       state.GameName.empty() ? "" : StrFmt("- %s", state.GameName.c_str()).c_str(),
                       (state.detectedQuirks.size() > 0) ? "(Q)" : "", state.isOptiPatcherSucceed ? "(OP)" : "");
        }

        if (ImGui::Begin(windowTitle.c_str(), NULL, flags))
        {
            bool rcasEnabled = false;

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
                    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)), "有可用更新：%s（当前 %s）",
                                       versionStatus.latestTag.c_str(), currentVersionText.c_str());

                    if (!versionStatus.latestUrl.empty())
                    {
                        ImGui::SameLine();
                        ImGui::TextLinkOpenURL("打开发布页面", versionStatus.latestUrl.c_str());
                    }

                    ImGui::Spacing();
                }
                else if (!versionStatus.error.empty())
                {
                    LOG_ERROR("Version check failed: {0}", versionStatus.error);
                    versionStatus.error.clear();
                }
                // Disabled error message
                // else if (!versionStatus.error.empty())
                //{
                //    ImGui::Spacing();
                //    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.4f, 0.f, 1.f)), "%s", versionStatus.error.c_str());
                //    ImGui::Spacing();
                //}
            }

            // No active upscaler message
            if (currentFeature == nullptr || !currentFeature->IsInited())
            {
                ImGui::Spacing();

                if (config->UseHQFont.value_or_default())
                    ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.5f));
                else
                    ImGui::SetWindowFontScale(menuResScale * 2.5f);

                if (state.nvngxExists || state.nvngxReplacement.has_value() ||
                    (state.libxessExists || XeSSProxy::Module() != nullptr))
                {
                    ImGui::Spacing();

                    std::vector<std::string> upscalers;

                    if (state.fsrHooks)
                        upscalers.push_back("FSR");

                    if (state.nvngxExists || state.nvngxReplacement.has_value() || state.isRunningOnNvidia)
                        upscalers.push_back("DLSS");

                    if (state.libxessExists || XeSSProxy::Module() != nullptr)
                        upscalers.push_back("XeSS");

                    auto joined = upscalers | std::views::join_with(std::string { " 或 " });

                    std::string joinedUpscalers(joined.begin(), joined.end());

                    ImGui::Text("请在游戏选项中选择 %s 作为超分器并载入存档\n"
                                "以启用 Opti 设置。\n超分器在菜单中不一定生效。",
                                joinedUpscalers.c_str());

                    if (config->UseHQFont.value_or_default())
                        ImGui::PopFontSize();
                    else
                        ImGui::SetWindowFontScale(menuResScale);

                    ImGui::Spacing();

                    if (!state.isRunningOnNvidia)
                    {
                        ImGui::Text("nvngx.dll：%s", state.nvngxExists ? "存在" : "不存在");
                    }

                    if (state.isRunningOnNvidia)
                    {
                        ImGui::Text("nvngx_dlss：%s", state.NVNGX_DLSS_Path.has_value() ? "存在" : "不存在");
                        ImGui::SameLine(0.0f, 16.0f);
                        ImGui::Text("nvngx_dlssd：%s",
                                    state.NVNGX_DLSSD_Path.has_value() ? "存在" : "不存在");
                    }
                    else
                    {
                        ImGui::SameLine(0.0f, 16.0f);
                        ImGui::Text("nvngx 替代：%s",
                                    state.nvngxReplacement.has_value() ? "存在" : "不存在");
                    }

                    ImGui::Text("libxess：%s",
                                (state.libxessExists || XeSSProxy::Module() != nullptr) ? "存在" : "不存在");

                    ImGui::Text("FSR 挂钩：%s", state.fsrHooks ? "存在" : "不存在");
                    ImGui::SameLine(0.0f, 16.0f);
                    ImGui::Text("FSR 3.1：%s", FfxApiProxy::Dx12Module() != nullptr ? "存在" : "不存在");
                    ImGui::SameLine(0.0f, 16.0f);
                    ImGui::Text("FSR 3.1 超分：%s", FfxApiProxy::Dx12Module_SR() != nullptr ? "存在" : "不存在");
                    ImGui::SameLine(0.0f, 16.0f);
                    ImGui::Text("FSR 3.1 帧生成：%s", FfxApiProxy::Dx12Module_FG() != nullptr ? "存在" : "不存在");

                    ImGui::Spacing();
                }
                else
                {
                    ImGui::Spacing();
                    ImGui::Text(
                        "找不到 nvngx.dll、libxess.dll 和 FSR 输入\n超分支持将无法工作。");
                    ImGui::Spacing();

                    if (config->UseHQFont.value_or_default())
                        ImGui::PopFont();
                    else
                        ImGui::SetWindowFontScale(menuResScale);
                }
            }
            else if (currentFeature->IsFrozen())
            {
                ImGui::Spacing();

                if (config->UseHQFont.value_or_default())
                    ImGui::PushFontSize(std::round(fontSize * menuResScale * 3.0f));
                else
                    ImGui::SetWindowFontScale(menuResScale * 3.0f);

                ImGui::Text("%s 已激活，但游戏当前未使用\n请进入游戏",
                            currentFeature->Name().c_str());

                if (config->UseHQFont.value_or_default())
                    ImGui::PopFont();
                else
                    ImGui::SetWindowFontScale(menuResScale);
            }

            if (ImGui::BeginTable("main", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    // UPSCALERS -----------------------------
                    ImGui::SeparatorText("超分器");
                    ShowTooltip("你要选哪种安慰剂？");

                    GetCurrentBackendInfo(state.api, &currentBackend, &currentBackendName);

                    std::string spoofingText;

                    ImGui::PushItemWidth(180.0f * menuResScale);

                    switch (state.api)
                    {
                    case DX11:
                        if (state.DeviceAdapterNames.contains(state.currentD3D11Device))
                            ImGui::Text(state.DeviceAdapterNames[state.currentD3D11Device].c_str());
                        else if (state.DeviceAdapterNames.contains(state.currentD3D12Device))
                            ImGui::Text(state.DeviceAdapterNames[state.currentD3D12Device].c_str());

                        ImGui::Text("D3D11 %s| %s %d.%d.%d", state.isRunningOnDXVK ? "(DXVK) " : "",
                                    currentFeature->Name().c_str(), currentFeature->Version().major,
                                    currentFeature->Version().minor, currentFeature->Version().patch);
                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::Text("| 输入：%s", state.currentInputApiName.c_str());

                        ImGui::SameLine(0.0f, 6.0f);
                        spoofingText = config->DxgiSpoofing.value_or_default() ? "开" : "关";
                        ImGui::Text("| 伪装：%s", spoofingText.c_str());

                        if (currentFeature->Name() != "DLSSD")
                            AddDx11Backends(&currentBackend, &currentBackendName);

                        break;

                    case DX12:
                        if (state.DeviceAdapterNames.contains(state.currentD3D12Device))
                            ImGui::Text(state.DeviceAdapterNames[state.currentD3D12Device].c_str());

                        ImGui::Text("D3D12 %s| %s %d.%d.%d", state.isRunningOnDXVK ? "(DXVK) " : "",
                                    currentFeature->Name().c_str(), currentFeature->Version().major,
                                    currentFeature->Version().minor, currentFeature->Version().patch);
                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::Text("| 输入：%s", state.currentInputApiName.c_str());

                        ImGui::SameLine(0.0f, 6.0f);
                        spoofingText = config->DxgiSpoofing.value_or_default() ? "开" : "关";
                        ImGui::Text("| 伪装：%s", spoofingText.c_str());

                        if (currentFeature->Name() != "DLSSD")
                            AddDx12Backends(&currentBackend, &currentBackendName);

                        break;

                    default:
                        if (state.DeviceAdapterNames.contains(state.currentVkDevice))
                            ImGui::Text(state.DeviceAdapterNames[state.currentVkDevice].c_str());

                        ImGui::Text("Vulkan %s| %s %d.%d.%d", state.isRunningOnDXVK ? "(DXVK) " : "",
                                    currentFeature->Name().c_str(), currentFeature->Version().major,
                                    currentFeature->Version().minor, currentFeature->Version().patch);
                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::Text("| 输入：%s", state.currentInputApiName.c_str());

                        auto vlkSpoof = config->VulkanSpoofing.value_or_default();
                        auto vlkExtSpoof = config->VulkanExtensionSpoofing.value_or_default();

                        if (vlkSpoof && vlkExtSpoof)
                            spoofingText = "开 + 扩展";
                        else if (vlkSpoof)
                            spoofingText = "开";
                        else if (vlkExtSpoof)
                            spoofingText = "仅扩展";
                        else
                            spoofingText = "关";

                        ImGui::SameLine(0.0f, 6.0f);
                        ImGui::Text("| 伪装：%s", spoofingText.c_str());

                        if (currentFeature->Name() != "DLSSD")
                            AddVulkanBackends(&currentBackend, &currentBackendName);
                    }

                    ImGui::PopItemWidth();

                    if (currentFeature->Name() != "DLSSD")
                    {
                        ImGui::SameLine(0.0f, 6.0f);

                        if (ImGui::Button("更换超分器##2") && state.newBackend != "" &&
                            state.newBackend != currentBackend)
                        {
                            if (state.newBackend == "xess")
                            {
                                // Reseting them for xess
                                config->DisableReactiveMask.reset();
                                config->DlssReactiveMaskBias.reset();
                            }

                            MARK_ALL_BACKENDS_CHANGED();
                        }
                    }

                    if (currentFeature->AccessToReactiveMask())
                    {
                        ImGui::BeginDisabled(config->DisableReactiveMask.value_or(false));

                        auto useAsTransparency = config->FsrUseMaskForTransparency.value_or_default();
                        if (ImGui::Checkbox("将反应蒙版用作透明度蒙版", &useAsTransparency))
                            config->FsrUseMaskForTransparency = useAsTransparency;

                        ImGui::EndDisabled();
                    }

                    if (state.isRunningOnNvidia && !state.NVNGX_DLSS_Path.has_value())
                    {
                        ImGui::Spacing();
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                           "未找到 nvngx_dlss.dll，DLSS 已禁用！");
                    }
                }

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    // Dx11 with Dx12
                    if (state.api == DX11 && config->Dx11Upscaler.value_or_default() != "fsr22" &&
                        config->Dx11Upscaler.value_or_default() != "dlss" &&
                        config->Dx11Upscaler.value_or_default() != "fsr31")
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
                    if (currentBackend == "xess" && currentFeature->Name() != "DLSSD")
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
                                        state.newBackend = currentBackend;
                                        MARK_ALL_BACKENDS_CHANGED();
                                    }
                                }

                                ImGui::EndCombo();
                            }
                            ShowHelpMarker("可能没什么作用");

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

                    // FFX -----------------
                    if (currentBackend.rfind("fsr", 0) == 0 && currentFeature->Name() != "DLSSD" &&
                        (currentBackend == "fsr31" || currentBackend == "fsr31_12"))
                    {
                        ImGui::SeparatorText("FFX 设置");

                        if (_ffxUpscalerIndex < 0)
                            _ffxUpscalerIndex = config->FfxUpscalerIndex.value_or_default();

                        if (currentBackend == "fsr31" ||
                            currentBackend == "fsr31_12" && state.ffxUpscalerVersionNames.size() > 0)
                        {
                            ImGui::PushItemWidth(135.0f * menuResScale);

                            auto currentName = StrFmt("FSR %s", state.ffxUpscalerVersionNames[_ffxUpscalerIndex]);
                            if (ImGui::BeginCombo("FFX 超分器", currentName.c_str()))
                            {
                                for (int n = 0; n < state.ffxUpscalerVersionIds.size(); n++)
                                {
                                    auto name = StrFmt("FSR %s", state.ffxUpscalerVersionNames[n]);
                                    if (ImGui::Selectable(name.c_str(),
                                                          config->FfxUpscalerIndex.value_or_default() == n))
                                        _ffxUpscalerIndex = n;
                                }

                                ImGui::EndCombo();
                            }
                            ImGui::PopItemWidth();

                            ShowHelpMarker("FFX SDK 报告的超分器列表");

                            ImGui::SameLine(0.0f, 6.0f);

                            if (ImGui::Button("更换超分器") &&
                                _ffxUpscalerIndex != config->FfxUpscalerIndex.value_or_default())
                            {
                                config->FfxUpscalerIndex = _ffxUpscalerIndex;
                                state.newBackend = currentBackend;
                                MARK_ALL_BACKENDS_CHANGED();
                            }

                            auto majorFsrVersion = currentFeature->Version().major;

                            if (majorFsrVersion >= 4)
                            {
                                ImGui::Spacing();

                                // Colorspaces
                                const char* colorSpaces[] = { "线性（默认）", "非线性", "非线性 sRGB",
                                                              "非线性 PQ" };
                                int currentColorSpace = 0;
                                if (config->FsrNonLinearPQ.value_or_default())
                                    currentColorSpace = 3;
                                else if (config->FsrNonLinearSRGB.value_or_default())
                                    currentColorSpace = 2;
                                else if (config->FsrNonLinearColorSpace.value_or_default())
                                    currentColorSpace = 1;

                                ImGui::SetNextItemWidth(150.0f * menuResScale);
                                if (ImGui::Combo("输入色彩空间", &currentColorSpace, colorSpaces,
                                                 IM_ARRAYSIZE(colorSpaces)))
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

                                    state.newBackend = currentBackend;
                                    MARK_ALL_BACKENDS_CHANGED();
                                }
                                ShowHelpMarker("选择游戏使用的输入色彩空间。\n"
                                               "非线性 / sRGB：可能改善 FSR4 超分质量，但也可能"
                                               "增加拖影。\n"
                                               "PQ：最罕见，可能增加拖影并破坏灯光。");

                                // FSR 4 Presets
                                const char* presets[] = { "默认",  "预设 0", "预设 1", "预设 2",
                                                          "预设 3", "预设 4", "预设 5" };
                                int currentPresetIdx =
                                    config->Fsr4Preset.has_value() ? config->Fsr4Preset.value() + 1 : 0;

                                if (currentPresetIdx < 0 || currentPresetIdx >= IM_ARRAYSIZE(presets))
                                    currentPresetIdx = 0;

                                ImGui::SetNextItemWidth(150.0f * menuResScale);
                                if (ImGui::Combo("FSR4 预设", &currentPresetIdx, presets, IM_ARRAYSIZE(presets)))
                                {
                                    if (currentPresetIdx == 0)
                                        config->Fsr4Preset.reset();
                                    else
                                        config->Fsr4Preset = currentPresetIdx - 1;

                                    state.newBackend = currentBackend;
                                    MARK_ALL_BACKENDS_CHANGED();
                                }
                                ShowHelpMarker(
                                    "每个内部 FSR4 预设都针对特定分辨率调优。\n"
                                    "选择 FSR4 预设不会改变游戏内的\n超分器预设！！！\n\n"
                                    "预设 0 适用于 FSR 原生抗锯齿\n"
                                    "预设 1 适用于质量/极致质量\n"
                                    "预设 2 适用于均衡\n"
                                    "预设 3 适用于性能\n"
                                    "预设 4 适用于 DRS\n"
                                    "预设 5 适用于极致性能");

                                // Display the active preset right next to the combo box instead of using a table
                                ImGui::SameLine();
                                if (state.currentFsr4Preset.has_value())
                                    ImGui::TextDisabled("（当前启用：%d）", state.currentFsr4Preset.value());
                                else if (FSR4ModelSelection::IsInt8FsrHooked())
                                    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                                       "（可能是 FSR3 回退）");
                                else
                                    ImGui::TextDisabled("（挂钩失败）");
                            }

                            if (majorFsrVersion >= 3)
                            {
                                ImGui::Spacing();

                                bool debugView = config->FsrDebugView.value_or_default();
                                if (ImGui::Checkbox("超分器调试视图", &debugView))
                                {
                                    config->FsrDebugView = debugView;

                                    if (majorFsrVersion > 3)
                                    {
                                        config->Fsr4EnableDebugView = debugView;
                                        state.newBackend = currentBackend;
                                        MARK_ALL_BACKENDS_CHANGED();
                                    }
                                }

                                if (majorFsrVersion > 3)
                                {
                                    ShowHelpMarker("左上：膨胀运动矢量\n"
                                                   "右上：预测混合因子");
                                }
                                else
                                {
                                    ShowHelpMarker("左上：膨胀运动矢量\n"
                                                   "中上：受保护区域\n"
                                                   "右上：膨胀深度\n"
                                                   "中间：超分后的画面\n"
                                                   "左下：去遮挡蒙版\n"
                                                   "中下：反应度\n"
                                                   "右下：细节保护衰减");
                                }

                                if (majorFsrVersion > 3)
                                {
                                    ImGui::SameLine(0.0f, 20.0f * menuResScale);
                                    bool fsr4wm = config->Fsr4EnableWatermark.value_or_default();
                                    if (ImGui::Checkbox("水印", &fsr4wm))
                                    {
                                        LOG_DEBUG("FSR4 Watermark set to {}", fsr4wm);
                                        config->Fsr4EnableWatermark = fsr4wm;
                                    }

                                    ShowHelpMarker("更改此选项后，请保存设置\n"
                                                   "它将在下次启动时生效。");
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

                                    // This wiill be applied by default
                                    if (ImGui::Button("稳定"))
                                    {
                                        auto const scaleRatioX = (float) currentFeature->TargetWidth() /
                                                                 (float) currentFeature->RenderWidth();
                                        auto const scaleRatioY = (float) currentFeature->TargetHeight() /
                                                                 (float) currentFeature->RenderHeight();
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
                                        auto const scaleRatioX = (float) currentFeature->TargetWidth() /
                                                                 (float) currentFeature->RenderWidth();
                                        auto const scaleRatioY = (float) currentFeature->TargetHeight() /
                                                                 (float) currentFeature->RenderHeight();
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
                                    if (ImGui::SliderFloat("速度因子", &velocity, 0.00f, 1.0f, "%.3f"))
                                        config->FsrVelocity = velocity;

                                    ShowHelpMarker("0.0f 可改善亮像素的时间稳定性\n"
                                                   "较低的值在拖影方面更稳定\n"
                                                   "较高的值更像素化但拖影更少。");

                                    if (currentFeature->Version() >= feature_version { 3, 1, 4 })
                                    {
                                        // Reactive Scale
                                        float reactiveScale = config->FsrReactiveScale.value_or_default();
                                        if (ImGui::SliderFloat("反应蒙版缩放", &reactiveScale, 0.0f, 1.0f, "%.3f"))
                                            config->FsrReactiveScale = reactiveScale;

                                        ShowHelpMarker("用于开发测试\n"
                                                       "向反应蒙版写入更大的值可减少拖影。");

                                        // Shading Scale
                                        float shadingScale = config->FsrShadingScale.value_or_default();
                                        if (ImGui::SliderFloat("着色缩放", &shadingScale, 0.0f, 1.0f, "%.3f"))
                                            config->FsrShadingScale = shadingScale;

                                        ShowHelpMarker("增大此值会缩放 FSR3.1 计算出的着色\n"
                                                       "在读取时改变该值可获得更高的反应性。");

                                        // Accumulation Added Per Frame
                                        float accAddPerFrame = config->FsrAccAddPerFrame.value_or_default();
                                        if (ImGui::SliderFloat("每帧累积增量", &accAddPerFrame, 0.00f, 1.0f,
                                                               "%.3f"))
                                            config->FsrAccAddPerFrame = accAddPerFrame;

                                        ShowHelpMarker(
                                            "对应每帧添加的累积量\n"
                                            "在发生去遮挡的像素坐标处，或当\n"
                                            "反应蒙版值 > 0.0f 时。减小此值并\n"
                                            "将拖影物体（即无运动矢量）绘制到反应蒙版\n"
                                            "将值调至接近 1.0f 可减少时间性拖影。\n"
                                            "减小此值可能导致更多细小特征像素闪烁。");

                                        // Min Disocclusion Accumulation
                                        float minDisOccAcc = config->FsrMinDisOccAcc.value_or_default();
                                        if (ImGui::SliderFloat("最小去遮挡累积量", &minDisOccAcc, -1.0f, 1.0f,
                                                               "%.3f"))
                                            config->FsrMinDisOccAcc = minDisOccAcc;

                                        ShowHelpMarker("增大此值可减少白色像素的时间性\n"
                                                       "闪烁，这些摆动的细物体正在去遮挡\n"
                                                       "且经常相互遮挡。过高的值可能增加拖影。");
                                    }

                                    ImGui::PopItemWidth();

                                    ImGui::Spacing();
                                    ImGui::Spacing();
                                }
                            }
                        }
                    }

                    // DLSS -----------------
                    if ((config->DLSSEnabled.value_or_default() && currentBackend == "dlss" &&
                         currentFeature->Version().major > 2) ||
                        currentFeature->Name() == "DLSSD")
                    {
                        const bool usesDlssd = currentFeature->Name() == "DLSSD";

                        if (usesDlssd)
                            ImGui::SeparatorText("DLSS-D 设置");
                        else
                            ImGui::SeparatorText("DLSS 设置");

                        auto overridden =
                            usesDlssd ? state.dlssdPresetsOverriddenExternally : state.dlssPresetsOverriddenExternally;

                        if (overridden)
                        {
                            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                               "预设已被外部覆盖");
                            ShowHelpMarker("这通常是因为使用了工具\n"
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

                            ShowHelpMarker("每个渲染预设各有优劣\n"
                                           "覆盖预设可能改善画质\n"
                                           "启用/禁用后请点击应用");

                            /*
                            auto currentPresetIndex = GetPresetIndex(currentFeature, true);

                            if (currentPresetIndex == 0)
                                ImGui::Text("Current Preset: Default");
                            else
                                ImGui::Text("Current Preset: %c", 64 + currentPresetIndex);
                            */

                            ImGui::BeginDisabled(
                                !config->DLSSDRenderPresetOverride.value_or_default() /*|| overridden*/);
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

                            ShowHelpMarker("每个渲染预设各有优劣\n"
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
                            LOG_DEBUG("Applying DLSS/DLSSD preset override changes, preset index: {}",
                                      comboPreset.value_or_default());

                            if (usesDlssd)
                            {
                                config->DLSSDRenderPresetForAll = comboPreset.value_or_default();
                                state.newBackend = "dlssd";
                            }
                            else
                            {
                                config->RenderPresetForAll = comboPreset.value_or_default();
                                state.newBackend = currentBackend;
                            }

                            MARK_ALL_BACKENDS_CHANGED();
                        }

                        ImGui::Spacing();

                        if (auto ch = ScopedCollapsingHeader(usesDlssd ? "高级 DLSS-D 设置"
                                                                       : "高级 DLSS 设置");
                            ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            bool appIdOverride = config->UseGenericAppIdWithDlss.value_or_default();
                            if (ImGui::Checkbox("对 DLSS 使用通用 App ID", &appIdOverride))
                                config->UseGenericAppIdWithDlss = appIdOverride;

                            ShowHelpMarker("对 NGX 使用通用 appid\n"
                                           "修复 OptiScaler 预设覆盖在部分游戏中无效的问题\n"
                                           "需要重启游戏。");

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

                /// FG INPUTS

                static std::vector<MenuOption<FGInput>> inputOptions;
                inputOptions.clear();

                // clang-format off

                inputOptions = {
                    { FGInput::NoFG, "无帧生成" },
                    { FGInput::Nukems, "Nukem 的 DLSSG",
                        "仅限 FSR3-FG\n\n开箱即支持 HUDless\n\n使用 Streamline 交换链进行节奏控制" },
                    { FGInput::FSRFG, "FSR 3.1 帧生成",
                        "可用于任何帧生成输出\n\n开箱即支持 HUDless" },
                    { FGInput::DLSSG, "通过 Streamline 的 DLSSG",
                        "可用于任何帧生成输出\n\n开箱即支持 HUDless\n\n仅限使用 Streamline v2 的游戏" },
                    { FGInput::XeFG, "XeFG" },
                    { FGInput::Upscaler, "OptiFG（超分器）",
                        "必须启用超分器\n\n可用于任何帧生成输出，但在某些情况下可能不完美\n\n为防止 UI 闪烁，需要 HUDfix" },
                    { FGInput::FSRFG30, "FSR 3.0 帧生成",
                        "可用于任何帧生成输出\n\n开箱即支持 HUDless" }
                };

                // clang-format on

                // XeFG requirements
                auto constexpr xefgInputIndex = (uint32_t) FGInput::XeFG;
                inputOptions[xefgInputIndex].set_disabled(true, "暂未支持");

                // OptiFG requirements
                auto constexpr optiFgIndex = (uint32_t) FGInput::Upscaler;
                inputOptions[optiFgIndex].set_disabled(state.api == API::DX11 || state.api == API::Vulkan,
                                                       "不支持的 API");
                inputOptions[optiFgIndex].set_disabled(state.workingMode == WorkingMode::Nvngx,
                                                       "不支持的 Opti 工作模式");

                if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::FSRFG &&
                    !FfxApiProxy::IsFGReady() && !fsr31InitTried)
                {
                    fsr31InitTried = true;
                    FfxApiProxy::InitFfxDx12();
                    inputOptions[optiFgIndex].set_disabled(!FfxApiProxy::IsFGReady(),
                                                           "缺少 amd_fidelityfx_dx12.dll");
                }
                else if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::XeFG &&
                         !xefgInitTried && XeFGProxy::Module() == nullptr)
                {
                    xefgInitTried = true;
                    XeFGProxy::InitXeFG();
                    inputOptions[optiFgIndex].set_disabled(XeFGProxy::Module() == nullptr, "缺少 libxess_fg.dll");
                }

                // DLSSG inputs requirements
                auto constexpr dlssgInputIndex = (uint32_t) FGInput::DLSSG;
                inputOptions[dlssgInputIndex].set_disabled(state.swapchainApi == API::DX11, "不支持的 API");

                if (!inputOptions[dlssgInputIndex].disabled && state.streamlineVersion.major < 2)
                {
                    inputOptions[dlssgInputIndex].set_disabled(
                        true, std::format("不支持的 Streamline 版本：{}.{}.{}", state.streamlineVersion.major,
                                          state.streamlineVersion.minor, state.streamlineVersion.patch));
                }

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
                    { FGOutput::NoFG, "无帧生成" },
                    { FGOutput::Nukems, "Nukem 版 FSR3-FG", "请在游戏内启用 DLSS-FG" },
                    { FGOutput::FSRFG, "FSR 帧生成", "FSR3/4 帧生成" },
                    { FGOutput::DLSSG, "DLSSG", "暂未支持" },
                    { FGOutput::XeFG, "XeFG", "XeFG" }
                };

                // clang-format on

                // DLSSG output requirements
                auto constexpr dlssgOutputIndex = (uint32_t) FGOutput::DLSSG;
                outputOptions[dlssgOutputIndex].set_disabled(true, "暂未支持");

                // Nukem's FG mod requirements
                auto constexpr nukemsInputIndex = (uint32_t) FGInput::Nukems;
                auto constexpr nukemsOutputIndex = (uint32_t) FGOutput::Nukems;
                if (state.workingMode == WorkingMode::Nvngx)
                {
                    inputOptions[nukemsInputIndex].set_disabled(true, "不支持的 Opti 工作模式");
                    outputOptions[nukemsOutputIndex].set_disabled(true, "不支持的 Opti 工作模式");
                }
                else if (!state.NukemsFilesAvailable)
                {
                    inputOptions[nukemsInputIndex].set_disabled(true,
                                                                "缺少 dlssg_to_fsr3_amd_is_better.dll 文件");
                    outputOptions[nukemsOutputIndex].set_disabled(true,
                                                                  "缺少 dlssg_to_fsr3_amd_is_better.dll 文件");
                }

                // FSR FG output requirements
                auto constexpr fsrfgOutputIndex = (uint32_t) FGOutput::FSRFG;
                outputOptions[fsrfgOutputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

                // XeFG output requirements
                auto constexpr xefgOutputIndex = (uint32_t) FGOutput::XeFG;
                outputOptions[xefgOutputIndex].set_disabled(state.swapchainApi != API::DX12, "不支持的 API");

                // Unsupported FG input selected
                if (config->FGInput != FGInput::NoFG && inputOptions[(uint32_t) state.activeFgInput].disabled &&
                    state.activeFgInput == config->FGInput)
                {
                    LOG_WARN("Resetting FGInput to NoFG: {}", inputOptions[(uint32_t) state.activeFgInput].label);
                    config->FGInput = FGInput::NoFG;
                }

                // Unsupported FG output selected
                if (config->FGOutput != FGOutput::NoFG && outputOptions[(uint32_t) state.activeFgOutput].disabled &&
                    state.activeFgOutput == config->FGOutput)
                {
                    LOG_WARN("Resetting FGOutput to NoFG: {}", outputOptions[(uint32_t) state.activeFgOutput].label);
                    config->FGOutput = FGOutput::NoFG;
                }

                if (!config->FGOutput.has_value())
                    config->FGOutput = config->FGOutput.value_or_default(); // need to have a value before combo

                {
                    ImGui::SeparatorText("帧生成");

                    if (ImGui::BeginTable("fgSelection", 2, ImGuiTableFlags_SizingStretchSame))
                    {
                        ImGui::TableNextColumn();

                        PopulateCombo("帧生成输入", config->FGInput, inputOptions);
                        ShowTooltip("用于帧生成的数据源\n"
                                    "游戏支持的原生帧生成");

                        ImGui::TableNextColumn();

                        const bool disableOutputs = config->FGInput.value_or_default() == FGInput::Nukems;

                        ImGui::BeginDisabled(disableOutputs);
                        PopulateCombo("帧生成输出", config->FGOutput, outputOptions);
                        ImGui::EndDisabled();

                        if (disableOutputs)
                            ShowTooltip("与所选帧生成源无关");
                        else
                            ShowTooltip("您实际将使用的帧生成");

                        ImGui::EndTable();
                    }

                    auto static fgInputOverridden = false;

                    if (config->FGOutput == FGOutput::Nukems && !fgInputOverridden)
                    {
                        config->FGInput = FGInput::Nukems;
                        fgInputOverridden = true;
                    }
                    else if (config->FGInput != FGInput::Nukems && fgInputOverridden)
                    {
                        config->FGOutput = FGOutput::NoFG;
                        fgInputOverridden = false;
                    }

                    state.fgSettingsChanged = state.activeFgOutput != config->FGOutput.value_or_default() ||
                                              state.activeFgInput != config->FGInput.value_or_default();

                    if (state.fgSettingsChanged)
                    {
                        ImGui::Spacing();
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.0f, 1.f)),
                                           "保存设置并重启游戏以应用更改");
                        ImGui::Spacing();
                    }

                    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
                    if (((state.activeFgOutput == FGOutput::FSRFG || state.activeFgOutput == FGOutput::XeFG) &&
                         state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::Nukems) &&
                        fgOutput)
                    {
                        ImGui::Checkbox("显示检测到的 UI", &state.FGHudlessCompare);
                        ShowHelpMarker("需要 HUDless 纹理与最终画面进行比较。\n"
                                       "UI 元素（且仅 UI 元素）应呈现粉色色调！");

                        const auto isUsingUIAny = fgOutput->IsUsingUIAny();

                        ImGui::BeginDisabled(!isUsingUIAny);

                        if (bool drawUIOverFG = config->FGDrawUIOverFG.value_or_default();
                            ImGui::Checkbox("在最终画面上绘制 UI", &drawUIOverFG))
                        {
                            config->FGDrawUIOverFG = drawUIOverFG;
                        }
                        ShowHelpMarker("将 UI 资源绘制到最终画面上\n"
                                       "如果看不到 UI，请启用此项！");

                        ImGui::EndDisabled();

                        ImGui::SameLine(0.0f, 16.0f);

                        ImGui::BeginDisabled(!isUsingUIAny || !config->FGDrawUIOverFG.value_or_default());

                        if (bool uiPremultipliedAlpha = config->FGUIPremultipliedAlpha.value_or_default();
                            ImGui::Checkbox("UI 预乘 Alpha", &uiPremultipliedAlpha))
                        {
                            config->FGUIPremultipliedAlpha = uiPremultipliedAlpha;
                        }
                        ShowHelpMarker("如果 UI 太淡，请禁用此选项");

                        ImGui::EndDisabled();
                    }

                    if (state.activeFgInput == FGInput::DLSSG || state.activeFgInput == FGInput::FSRFG ||
                        state.activeFgInput == FGInput::FSRFG30)
                    {
                        ImGui::Spacing();

                        if (auto ch = ScopedCollapsingHeader("高级帧生成设置"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
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

                                ShowHelpMarker("当游戏发送 UI 纹理但您想禁用它时");

                                ImGui::EndDisabled();

                                ImGui::SameLine(0.0f, 16.0f);

                                bool disableHudless = config->FGDisableHudless.value_or_default();
                                ImGui::BeginDisabled(!isUsingHudlessAny && !disableHudless);

                                if (ImGui::Checkbox("禁用 HUDless", &disableHudless))
                                {
                                    config->FGDisableHudless = disableHudless;
                                }

                                ShowHelpMarker("当游戏发送 HUDless 但您想禁用它时");

                                ImGui::EndDisabled();

                                bool depthValidNow = config->FGDepthValidNow.value_or_default();
                                if (ImGui::Checkbox("将深度作为 ValidNow", &depthValidNow))
                                    config->FGDepthValidNow = depthValidNow;

                                ShowHelpMarker("会占用更多显存，但 Uniscaler 需要此项\n"
                                               "也许其他一些游戏也需要");

                                ImGui::SameLine(0.0f, 16.0f);

                                bool velocityValidNow = config->FGVelocityValidNow.value_or_default();
                                if (ImGui::Checkbox("将运动矢量作为 ValidNow", &velocityValidNow))
                                    config->FGVelocityValidNow = velocityValidNow;

                                ShowHelpMarker("会占用更多显存，但 Uniscaler 需要此项\n"
                                               "也许其他一些游戏也需要");

                                bool hudlessValidNow = config->FGHudlessValidNow.value_or_default();
                                if (ImGui::Checkbox("将 HUDless 作为 ValidNow", &hudlessValidNow))
                                    config->FGHudlessValidNow = hudlessValidNow;

                                ShowHelpMarker("会占用更多显存，但某些游戏可能需要此项");

                                ImGui::SameLine(0.0f, 16.0f);

                                bool firstHudless = config->FGOnlyAcceptFirstHudless.value_or_default();
                                if (ImGui::Checkbox("接受首个 HUDless", &firstHudless))
                                    config->FGOnlyAcceptFirstHudless = firstHudless;

                                ShowHelpMarker("如果源标记了多个 HUDless，仅使用第一个");

                                if (bool skipReset = config->FGSkipReset.value_or_default();
                                    ImGui::Checkbox("跳过重置", &skipReset))
                                {
                                    config->FGSkipReset = skipReset;
                                }

                                ShowHelpMarker("不使用帧生成输入的复位信号");

                                ImGui::EndDisabled();

                                ImGui::PushItemWidth(80.0f * menuResScale);

                                auto frameAhead = config->FGAllowedFrameAhead.value_or_default();
                                if (ImGui::InputInt("领先帧数", &frameAhead, 1, 1) && frameAhead > 0 &&
                                    frameAhead < 4)
                                {
                                    config->FGAllowedFrameAhead = frameAhead;
                                }

                                ShowHelpMarker("帧生成允许领先于游戏的帧数\n"
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
                                            LOG_DEBUG("FTInput has changed {} -> {}", ftSources[currentSet],
                                                      ftSources[i]);
                                            config->FTInput = (FrameTimeSource) i;
                                        }

                                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                            ImGui::SetTooltip(ftSourceInfos[i]);
                                    }

                                    ImGui::EndCombo();
                                }

                                ImGui::PopItemWidth();

                                ShowHelpMarker("选择帧时间来源\n"
                                               "可能有助于帧节奏和卡顿问题");
                            }
                        }
                    }

                    ImGui::Spacing();
                }

                // FSR FG controls
                if (state.activeFgOutput == FGOutput::FSRFG && state.activeFgInput != FGInput::NoFG &&
                    state.workingMode != WorkingMode::Nvngx && state.currentFGSwapchain != nullptr)
                {
                    if (state.activeFgInput != FGInput::Upscaler ||
                        (currentFeature != nullptr && !currentFeature->IsFrozen()) && FfxApiProxy::IsFGReady())
                    {
                        ImGui::SeparatorText("帧生成（FSR FG）");

                        if (_ffxFGIndex < 0)
                            _ffxFGIndex = config->FfxFGIndex.value_or_default();

                        if (state.ffxFGVersionNames.size() > 0)
                        {
                            ImGui::PushItemWidth(135.0f * menuResScale);

                            auto currentName = StrFmt("FSR %s", state.ffxFGVersionNames[_ffxFGIndex]);
                            if (ImGui::BeginCombo("FFX FG", currentName.c_str()))
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

                            ShowHelpMarker("FFX SDK 报告的帧生成器列表");

                            ImGui::SameLine(0.0f, 6.0f);

                            if (ImGui::Button("更换帧生成") && _ffxFGIndex != config->FfxFGIndex.value_or_default())
                            {
                                config->FfxFGIndex = _ffxFGIndex;
                                state.FGchanged = true;
                                state.SCchanged = true;
                            }
                        }

                        bool fgActive = config->FGEnabled.value_or_default();
                        if (ImGui::Checkbox("启用##2", &fgActive))
                        {
                            config->FGEnabled = fgActive;
                            LOG_DEBUG("FGEnabled set FGEnabled: {}", fgActive);

                            if (config->FGEnabled.value_or_default())
                                state.FGchanged = true;
                        }
                        ShowHelpMarker("启用帧生成");

                        bool fgAsync = config->FGAsync.value_or_default();
                        if (ImGui::Checkbox("允许异步", &fgAsync))
                        {
                            config->FGAsync = fgAsync;

                            if (config->FGEnabled.value_or_default())
                            {
                                state.FGchanged = true;
                                state.SCchanged = true;
                                LOG_DEBUG("Async set FGChanged");
                            }
                        }
                        ShowHelpMarker(
                            "启用异步可获得更好的帧生成性能\n可能导致崩溃，尤其在使用 HUD 修复时！");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool fgDV = config->FGDebugView.value_or_default();
                        if (ImGui::Checkbox("调试视图##2", &fgDV))
                        {
                            config->FGDebugView = fgDV;

                            if (config->FGEnabled.value_or_default())
                            {
                                state.FGchanged = true;
                                LOG_DEBUG("DebugView set FGChanged");
                            }
                        }
                        ShowHelpMarker("启用 FSR3.1-FG 调试视图\n\n"
                                       "左上：游戏运动矢量\n"
                                       "中上：GMV 深度\n"
                                       "右上：光流运动矢量\n"
                                       "中间：仅插值帧\n"
                                       "左下：去遮挡蒙版\n"
                                       "中下：插值来源（无 UI）\n"
                                       "右下：HUDless 资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        if (state.currentFG->Version().major > 3)
                        {
                            if (bool fgwm = config->FSRFGEnableWatermark.value_or_default();
                                ImGui::Checkbox("帧生成水印", &fgwm))
                            {
                                LOG_DEBUG("FSRFGEnableWatermark set FGWatermark: {}", fgwm);
                                config->FSRFGEnableWatermark = fgwm;
                            }

                            ShowHelpMarker("更改此选项后，请保存设置\n"
                                           "它将在下次启动时生效。");
                        }

                        ImGui::Spacing();
                        ImGui::Spacing();
                        if (auto ch = ScopedCollapsingHeader("高级 FSR FG 设置"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            ImGui::Checkbox("仅帧生成的帧", &state.FGonlyGenerated);
                            ShowHelpMarker("仅显示 FSR 3.1 生成的帧");

                            ImGui::SameLine(0.0f, 16.0f);
                            auto debugResetLines = config->FGDebugResetLines.value_or_default();
                            if (ImGui::Checkbox("调试重置线", &debugResetLines))
                            {
                                config->FGDebugResetLines = debugResetLines;
                                LOG_DEBUG("Enabled set FGDebugLines: {}", debugResetLines);
                            }
                            ShowHelpMarker("启用绘制插值跳过线");

                            auto debugTearLines = config->FGDebugTearLines.value_or_default();
                            if (ImGui::Checkbox("调试撕裂线", &debugTearLines))
                            {
                                config->FGDebugTearLines = debugTearLines;
                                LOG_DEBUG("Enabled set FGDebugLines: {}", debugTearLines);
                            }
                            ShowHelpMarker("启用绘制撕裂与插值跳过线");

                            ImGui::SameLine(0.0f, 16.0f);
                            auto debugPacingLines = config->FGDebugPacingLines.value_or_default();
                            if (ImGui::Checkbox("调试节奏线", &debugPacingLines))
                            {
                                config->FGDebugPacingLines = debugPacingLines;
                                LOG_DEBUG("Enabled set FGDebugLines: {}", debugPacingLines);
                            }
                            ShowHelpMarker("启用绘制节奏线");

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
                                ShowHelpMarker("帧生成区域，为黑边内容调整");

                                ImGui::BeginDisabled(
                                    !config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                    !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                                if (ImGui::Button("重置帧生成区域"))
                                {
                                    config->FGRectLeft.reset();
                                    config->FGRectTop.reset();
                                    config->FGRectWidth.reset();
                                    config->FGRectHeight.reset();
                                }

                                ShowHelpMarker("重置帧生成区域");

                                ImGui::EndDisabled();
                                ImGui::TreePop();
                            }

                            auto fg = state.currentFG;
                            if (fg != nullptr && strcmp(fg->Name(), "FSR-FG") == 0 &&
                                FfxApiProxy::VersionDx12_FG() >= feature_version { 3, 1, 3 })
                            {
                                ImGui::Spacing();

                                if (ImGui::TreeNode("帧节奏调优"))
                                {
                                    auto fptEnabled = config->FGFramePacingTuning.value_or_default();
                                    if (ImGui::Checkbox("启用调优", &fptEnabled))
                                    {
                                        config->FGFramePacingTuning = fptEnabled;
                                        state.FSRFGFTPchanged = true;
                                    }

                                    ImGui::BeginDisabled(!config->FGFramePacingTuning.value_or_default());

                                    ImGui::PushItemWidth(115.0f * menuResScale);
                                    auto fptSafetyMargin = config->FGFPTSafetyMarginInMs.value_or_default();
                                    if (ImGui::InputFloat("安全余量（毫秒）", &fptSafetyMargin, 0.01f, 0.1f,
                                                          "%.2f"))
                                        config->FGFPTSafetyMarginInMs = fptSafetyMargin;
                                    ShowHelpMarker("安全余量（毫秒）\n"
                                                   "FSR 默认值：0.1ms\n"
                                                   "Opti 默认值：0.01ms");

                                    auto fptVarianceFactor = config->FGFPTVarianceFactor.value_or_default();
                                    if (ImGui::SliderFloat("方差因子", &fptVarianceFactor, 0.0f, 1.0f, "%.2f"))
                                        config->FGFPTVarianceFactor = fptVarianceFactor;
                                    ShowHelpMarker("方差因子\n"
                                                   "FSR 默认值：0.1\n"
                                                   "Opti 默认值：0.3");
                                    ImGui::PopItemWidth();

                                    auto fpHybridSpin = config->FGFPTAllowHybridSpin.value_or_default();
                                    if (ImGui::Checkbox("启用混合自旋", &fpHybridSpin))
                                        config->FGFPTAllowHybridSpin = fpHybridSpin;
                                    ShowHelpMarker("允许节奏自旋锁休眠，可降低 CPU 占用\n"
                                                   "可能导致 FPS 爬升缓慢");

                                    ImGui::PushItemWidth(115.0f * menuResScale);
                                    auto fptHybridSpinTime = config->FGFPTHybridSpinTime.value_or_default();
                                    if (ImGui::SliderInt("混合自旋时间", &fptHybridSpinTime, 0, 100))
                                        config->FGFPTHybridSpinTime = fptHybridSpinTime;
                                    ShowHelpMarker("当 FPTHybridSpin 为 true 时自旋的时间。以计时器"
                                                   "分辨率单位计量。\n"
                                                   "不建议低于 2，否则会导致频繁过冲");
                                    ImGui::PopItemWidth();

                                    auto fpWaitForSingleObjectOnFence =
                                        config->FGFPTAllowWaitForSingleObjectOnFence.value_or_default();
                                    if (ImGui::Checkbox("启用 WaitForSingleObjectOnFence",
                                                        &fpWaitForSingleObjectOnFence))
                                    {
                                        config->FGFPTAllowWaitForSingleObjectOnFence = fpWaitForSingleObjectOnFence;
                                    }
                                    ShowHelpMarker("允许使用 WaitForSingleObject 代替自旋等待围栏值");

                                    if (ImGui::Button("应用时序更改"))
                                        state.FSRFGFTPchanged = true;

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
                if (state.activeFgOutput == FGOutput::XeFG && state.activeFgInput != FGInput::NoFG &&
                    state.workingMode != WorkingMode::Nvngx && state.currentFGSwapchain != nullptr)
                {
                    if (XeFGProxy::InitXeFG() && currentFeature != nullptr && !currentFeature->IsFrozen())
                    {
                        ImGui::SeparatorText("帧生成（XeFG）");

                        bool ignoreChecks = config->FGXeFGIgnoreInitChecks.value_or_default();

                        bool nativeAA = false;
                        if (state.activeFgInput == FGInput::Upscaler && currentFeature != nullptr)
                            nativeAA = currentFeature->RenderWidth() == currentFeature->DisplayWidth();

                        auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
                        const bool correctMVs = fgOutput && fgOutput->IsLowResMV() || nativeAA ||
                                                (State::Instance().gameQuirks & GameQuirk::ForceFGRenderSizeMVs) ||
                                                ignoreChecks;

                        if (!correctMVs || state.realExclusiveFullscreen)
                        {
                            config->FGEnabled.reset();
                            config->FGXeFGDebugView.reset();
                        }

                        const bool restartNeeded =
                            fgOutput &&
                            (config->FGXeFGDepthInverted.value_or_default() != fgOutput->IsInvertedDepth() ||
                             config->FGXeFGJitteredMV.value_or_default() != fgOutput->IsJitteredMVs() ||
                             config->FGXeFGHighResMV.value_or_default() == fgOutput->IsLowResMV());

                        bool cantActivate = false;
                        if (restartNeeded)
                        {
                            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                               "请重启游戏以应用正确的 XeFG 设置！");
                        }
                        else
                        {
                            if (!correctMVs)
                                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                                   "需要禁用膨胀运动矢量");

                            if (!ignoreChecks && state.realExclusiveFullscreen)
                            {
                                cantActivate = true;
                                ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                                   "需要无边框显示模式！");
                            }

                            if (!ignoreChecks && state.isHdrActive)
                            {
                                if (state.currentSwapchainDesc.BufferDesc.Format > 0 &&
                                    state.currentSwapchainDesc.BufferDesc.Format < 15)
                                {
                                    cantActivate = true;
                                    ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.0f, 0.0f, 1.f)),
                                                       "XeFG 仅支持 HDR10");
                                }
                            }
                        }

                        if (!correctMVs || cantActivate || ignoreChecks)
                        {
                            if (ImGui::Checkbox("忽略初始化检查", &ignoreChecks))
                                config->FGXeFGIgnoreInitChecks = ignoreChecks;

                            ShowHelpMarker("忽略 XeFG 的所有预检查\n"
                                           "不要使用此选项跳过 UE 游戏的运动矢量大小警告！\n"
                                           "它可能导致崩溃和画质变差！");
                        }

                        ImGui::BeginDisabled(!correctMVs || cantActivate);

                        bool fgActive = config->FGEnabled.value_or_default();
                        if (ImGui::Checkbox("启用##3", &fgActive))
                        {
                            config->FGEnabled = fgActive;
                            LOG_DEBUG("Enabled set FGEnabled: {}", fgActive);

                            if (config->FGEnabled.value_or_default())
                                state.FGchanged = true;
                        }

                        ShowHelpMarker("启用帧生成");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto maxInterpolationCount = state.xefgMaxInterpolationCount;

                        if (maxInterpolationCount > 1)
                        {
                            const char* intModes[] = { "2X", "3X", "4X", "5X", "6X" };
                            auto currentSet = config->FGXeFGInterpolationCount.value_or_default() - 1;
                            auto currentIntCount = intModes[currentSet];

                            ImGui::PushItemWidth(95.0f * menuResScale);

                            if (ImGui::BeginCombo("MFG", currentIntCount))
                            {
                                for (int i = 0; i < maxInterpolationCount; i++)
                                {
                                    if (ImGui::Selectable(intModes[i], (currentSet == i)))
                                    {
                                        LOG_DEBUG("XeFG Interpolation Count set to: {}", i + 1);
                                        state.FGchanged = true;
                                        config->FGXeFGInterpolationCount = i + 1;
                                    }
                                }

                                ImGui::EndCombo();
                            }

                            ImGui::PopItemWidth();

                            ShowHelpMarker("设置 XeFG 插值数量");
                        }

                        ImGui::SameLine(0.0f, 16.0f);
                        ImGui::BeginDisabled(!fgOutput->IsUsingHudlessAny() ||
                                             XeFGProxy::SetUiCompositionState() == nullptr);
                        bool fgCompositeUI = config->FGXeFGUIComposition.value_or_default();
                        if (ImGui::Checkbox("UI 合成", &fgCompositeUI))
                            config->FGXeFGUIComposition = fgCompositeUI;

                        ShowHelpMarker("禁用 HUD/UI 插值\n"
                                       "恢复为之前的 XeFG 2 行为\n\n"
                                       "修复透明 HUD/UI 的伪影问题");
                        ImGui::EndDisabled();

                        bool fgDV = config->FGXeFGDebugView.value_or_default();
                        if (ImGui::Checkbox("调试视图##2", &fgDV))
                        {
                            config->FGXeFGDebugView = fgDV;

                            if (config->FGXeFGDebugView.value_or_default())
                            {
                                state.FGchanged = true;
                                LOG_DEBUG("DebugView set FGChanged");
                            }
                        }
                        ShowHelpMarker("启用 XeFG 调试视图");

                        ImGui::EndDisabled();

                        ImGui::SameLine(0.0f, 16.0f);
                        bool fgBorderless = config->FGXeFGForceBorderless.value_or_default();
                        if (ImGui::Checkbox("强制无边框", &fgBorderless))
                            config->FGXeFGForceBorderless = fgBorderless;

                        ShowHelpMarker("强制无边框显示模式\n\n"
                                       "为获得最佳效果，请设置为全屏\n"
                                       "分辨率与您的显示分辨率一致\n"
                                       "可能导致一些不稳定问题。\n\n"
                                       "需要重启游戏才能生效！");

                        // Disable this for now
                        // ImGui::SameLine(0.0f, 16.0f);
                        // ImGui::Checkbox("Only Generated##2", &state.FGonlyGenerated);
                        // ShowHelpMarker("Display only XeFG generated frames");

                        ImGui::Spacing();
                        if (auto ch = ScopedCollapsingHeader("高级 XeFG 设置"); ch.IsHeaderOpen())
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
                                ShowHelpMarker("帧生成区域，为黑边内容调整##2");

                                ImGui::BeginDisabled(
                                    !config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                    !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                                if (ImGui::Button("重置帧生成区域##2"))
                                {
                                    config->FGRectLeft.reset();
                                    config->FGRectTop.reset();
                                    config->FGRectWidth.reset();
                                    config->FGRectHeight.reset();
                                }

                                ShowHelpMarker("重置帧生成区域##2");

                                ImGui::EndDisabled();
                                ImGui::TreePop();
                            }

                            ImGui::Spacing();
                            ImGui::Spacing();
                        }
                    }
                }

                // OptiFG
                if (state.api == DX12 && state.currentFGSwapchain != nullptr &&
                    state.workingMode != WorkingMode::Nvngx && state.activeFgInput == FGInput::Upscaler)
                {
                    SeparatorWithHelpMarker("帧生成（OptiFG）", "使用超分器数据进行帧生成");

                    if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
                        ((state.activeFgOutput == FGOutput::FSRFG && FfxApiProxy::IsFGReady()) ||
                         (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() != nullptr)))
                    {
                        if (!Config::Instance()->FGDisableHUDFix.value_or_default())
                        {
                            bool fgHudfix = config->FGHUDFix.value_or_default();

                            if (ImGui::Checkbox("HUDFix", &fgHudfix))
                            {
                                config->FGHUDFix = fgHudfix;
                                LOG_DEBUG("Enabled set FGHUDFix: {}", fgHudfix);
                                state.ClearCapturedHudlesses = true;
                                state.FGchanged = true;
                            }

                            ShowHelpMarker("启用 HUD 稳定性修复，可能导致崩溃！");

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
                                LOG_DEBUG("Enabled set FGHUDLimit: {}", hudFixLimit);
                            }
                            ShowHelpMarker("延迟 HUDless 捕获，过高的值可能导致崩溃！");

                            ImGui::SameLine(0.0f, 16.0f);
                            if (ImGui::Button("重置##2"))
                                _showHudlessWindow = !_showHudlessWindow;

                            ImGui::EndDisabled();

                            auto hudExtended = config->FGHUDFixExtended.value_or_default();
                            if (ImGui::Checkbox("扩展", &hudExtended))
                            {
                                LOG_DEBUG("Enabled set FGHUDFixExtended: {}", hudExtended);
                                config->FGHUDFixExtended = hudExtended;
                            }
                            ShowHelpMarker(
                                "对可能的 HUDless 进行扩展格式检查\n可能导致崩溃和卡顿！");
                            ImGui::SameLine(0.0f, 16.0f);

                            ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                            auto immediate = config->FGImmediateCapture.value_or_default();
                            if (ImGui::Checkbox("立即捕获", &immediate))
                            {
                                LOG_DEBUG("Enabled set FGImmediateCapture: {}", immediate);
                                config->FGImmediateCapture = immediate;
                            }
                            ShowHelpMarker("在着色器执行前捕获资源。\n提高 HUDless"
                                           "捕获几率，但可能捕获不必要的资源。");

                            ImGui::PopItemWidth();

                            ImGui::EndDisabled();
                        }
                        bool depthScale = config->FGEnableDepthScale.value_or_default();
                        if (ImGui::Checkbox("缩放深度以修复 DLSS RR", &depthScale))
                            config->FGEnableDepthScale = depthScale;
                        ShowHelpMarker("修复 DLSS-D 错误的深度输入");

                        bool resourceFlip = config->FGResourceFlip.value_or_default();
                        if (ImGui::Checkbox("翻转（Unity）", &resourceFlip))
                            config->FGResourceFlip = resourceFlip;
                        ShowHelpMarker("翻转 Unity 游戏的速度与深度资源");

                        ImGui::SameLine(0.0f, 16.0f);

                        bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
                        if (ImGui::Checkbox("翻转使用偏移", &resourceFlipOffset))
                            config->FGResourceFlipOffset = resourceFlipOffset;
                        ShowHelpMarker("使用高度差作为偏移");

                        ImGui::Spacing();

                        if (auto ch = ScopedCollapsingHeader("高级 OptiFG 设置"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};

                            if (!Config::Instance()->FGDisableHUDFix.value_or_default())
                            {
                                ImGui::Spacing();

                                auto rb = config->FGResourceBlocking.value_or_default();
                                if (ImGui::Checkbox("资源阻止", &rb))
                                {
                                    config->FGResourceBlocking = rb;
                                    LOG_DEBUG("Enabled set FGResourceBlocking: {}", rb);
                                }
                                ShowHelpMarker("阻止不常用资源被用作 HUDless\n"
                                               "以防止闪烁和其他问题\n\n"
                                               "启用/禁用 HUDfix 将重置阻止列表！");

                                ImGui::SameLine(0.0f, 16.0f);

                                auto rrc = config->FGRelaxedResolutionCheck.value_or_default();
                                if (ImGui::Checkbox("放宽资源检查", &rrc))
                                {
                                    config->FGRelaxedResolutionCheck = rrc;
                                    LOG_DEBUG("Enabled set FGRelaxedResolutionCheck: {}", rrc);
                                }
                                ShowHelpMarker("将 HUDless 的分辨率检查放宽 32 像素\n"
                                               "帮助某些使用黑边的游戏\n"
                                               "以及特殊分辨率和屏幕比例（例如《巫师3》）");

                                ImGui::BeginDisabled(state.FGresetCapturedResources);
                                ImGui::PushItemWidth(95.0f * menuResScale);
                                if (ImGui::Checkbox("帧生成创建列表", &state.FGcaptureResources))
                                {
                                    if (!state.FGcaptureResources)
                                        config->FGHUDLimit = 1;
                                    else
                                        state.FGonlyUseCapturedResources = false;
                                }

                                ImGui::SameLine(0.0f, 16.0f);
                                if (ImGui::Checkbox("帧生成使用列表", &state.FGonlyUseCapturedResources))
                                {
                                    if (state.FGcaptureResources)
                                    {
                                        state.FGcaptureResources = false;
                                        config->FGHUDLimit = 1;
                                    }
                                }

                                ImGui::SameLine(0.0f, 8.0f);
                                ImGui::Text("(%d)", state.FGcapturedResourceCount);

                                ImGui::PopItemWidth();

                                ImGui::SameLine(0.0f, 16.0f);

                                if (ImGui::Button("重置列表"))
                                {
                                    LOG_DEBUG("Resetting captured resource list");

                                    state.FGresetCapturedResources = true;
                                    state.FGonlyUseCapturedResources = false;
                                }

                                ImGui::EndDisabled();

                                ImGui::Spacing();
                                ImGui::Spacing();
                                if (ImGui::TreeNode("跟踪设置"))
                                {
                                    auto ath = config->FGAlwaysTrackHeaps.value_or_default();
                                    if (ImGui::Checkbox("始终跟踪堆", &ath))
                                    {
                                        config->FGAlwaysTrackHeaps = ath;
                                        LOG_DEBUG("Enabled set FGAlwaysTrackHeaps: {}", ath);
                                    }
                                    ShowHelpMarker(
                                        "始终跟踪资源，可能导致性能问题\n，但也可能"
                                        "修复与 HUDfix 相关的崩溃！");

                                    auto disableRTV = config->FGHudfixDisableRTV.value_or_default();
                                    if (ImGui::Checkbox("禁用 RTV 跟踪", &disableRTV))
                                        config->FGHudfixDisableRTV = disableRTV;
                                    ShowHelpMarker("禁用 CreateRenderTargetView 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    ImGui::SameLine(0.0f, 16.0f);

                                    auto disableSRV = config->FGHudfixDisableSRV.value_or_default();
                                    if (ImGui::Checkbox("禁用 SRV 跟踪", &disableSRV))
                                        config->FGHudfixDisableSRV = disableSRV;
                                    ShowHelpMarker("禁用 CreateShaderResourceView 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    auto disableUAV = config->FGHudfixDisableUAV.value_or_default();
                                    if (ImGui::Checkbox("禁用 UAV 跟踪", &disableUAV))
                                        config->FGHudfixDisableUAV = disableUAV;
                                    ShowHelpMarker("禁用 CreateUnorderedAccessView 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    ImGui::SameLine(0.0f, 16.0f);

                                    auto disableOM = config->FGHudfixDisableOM.value_or_default();
                                    if (ImGui::Checkbox("禁用 OM 跟踪", &disableOM))
                                        config->FGHudfixDisableOM = disableOM;
                                    ShowHelpMarker("禁用 OMSetRenderTargets 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    auto disableSCR = config->FGHudfixDisableSCR.value_or_default();
                                    if (ImGui::Checkbox("禁用 SCR 跟踪", &disableSCR))
                                        config->FGHudfixDisableSCR = disableSCR;
                                    ShowHelpMarker("禁用 SetComputeRootDescriptorTable 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    ImGui::SameLine(0.0f, 16.0f);

                                    auto disableSGR = config->FGHudfixDisableSGR.value_or_default();
                                    if (ImGui::Checkbox("禁用 SGR 跟踪", &disableSGR))
                                        config->FGHudfixDisableSGR = disableSGR;
                                    ShowHelpMarker("禁用 SetGraphicsRootDescriptorTable 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    ImGui::Spacing();

                                    auto disableDI = config->FGHudfixDisableDI.value_or_default();
                                    if (ImGui::Checkbox("禁用 DI 跟踪", &disableDI))
                                        config->FGHudfixDisableDI = disableDI;
                                    ShowHelpMarker("禁用 DrawInstanced 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    ImGui::SameLine(0.0f, 16.0f);

                                    auto disableDII = config->FGHudfixDisableDII.value_or_default();
                                    if (ImGui::Checkbox("禁用 DII 跟踪", &disableDII))
                                        config->FGHudfixDisableDII = disableDII;
                                    ShowHelpMarker("禁用 DrawIndexedInstanced 的跟踪\n"
                                                   "这可能有助于过滤错误的 HUDless 资源");

                                    auto disableDispatch = config->FGHudfixDisableDispatch.value_or_default();
                                    if (ImGui::Checkbox("禁用 Dispatch 跟踪", &disableDispatch))
                                        config->FGHudfixDisableDispatch = disableDispatch;
                                    ShowHelpMarker("禁用 Dispatch 的跟踪\n"
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
                                ShowHelpMarker("复制运动矢量以供 OptiFG 使用\n"
                                               "以防止可能发生的损坏");

                                bool makeDepthCopies = config->FGMakeDepthCopy.value_or_default();
                                if (ImGui::Checkbox("帧生成复制深度", &makeDepthCopies))
                                    config->FGMakeDepthCopy = makeDepthCopies;
                                ShowHelpMarker("复制深度以供 OptiFG 使用\n"
                                               "以防止可能发生的损坏");

                                ImGui::PushItemWidth(115.0f * menuResScale);
                                float depthScaleMax = config->FGDepthScaleMax.value_or_default();
                                if (ImGui::InputFloat("帧生成深度缩放上限", &depthScaleMax, 10.0f, 100.0f, "%.1f"))
                                    config->FGDepthScaleMax = depthScaleMax;
                                ShowHelpMarker("深度值将除以该值");
                                ImGui::PopItemWidth();

                                ImGui::TreePop();
                            }

                            ImGui::Spacing();
                            if (ImGui::TreeNode("同步设置"))
                            {
                                bool useMutexForPresent = config->FGUseMutexForSwapchain.value_or_default();
                                if (ImGui::Checkbox("呈现时使用互斥锁", &useMutexForPresent))
                                    config->FGUseMutexForSwapchain = useMutexForPresent;
                                ShowHelpMarker("使用互斥锁防止帧生成失同步和崩溃\n"
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
                    else if (state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFGReady())
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

                // Nukems Mod
                if (state.workingMode != WorkingMode::Nvngx && state.activeFgInput == FGInput::Nukems &&
                    state.activeFgOutput == FGOutput::Nukems)
                {
                    SeparatorWithHelpMarker("帧生成（通过 Nukem 的 DLSSG 使用 FSR3-FG）",
                                            "需要 Nukem 的 dlssg_to_fsr3 dll\n请在游戏内选择 DLSS-FG");

                    if (!state.NukemsFilesAvailable)
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                           "请将 dlssg_to_fsr3_amd_is_better.dll 放在 OptiScaler 旁边");

                    if (!ReflexHooks::isReflexHooked())
                    {
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "Reflex 未挂钩");
                        ImGui::Text("如果您使用的是 AMD/Intel 显卡，请确保已安装 Fakenvapi");
                    }
                    else if (!ReflexHooks::isDlssgDetected())
                    {
                        ImGui::Text("请在游戏选项中启用 DLSS 帧生成\n"
                                    "您可能需要先选择 DLSS");
                    }

                    if (state.swapchainApi == DX12)
                    {
                        ImGui::Text("当前 DLSSG 状态：");
                        ImGui::SameLine();
                        if (ReflexHooks::isDlssgDetected())
                            ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                        else
                            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");

                        if (bool makeDepthCopy = config->MakeDepthCopy.value_or_default();
                            ImGui::Checkbox("修复损坏的画面", &makeDepthCopy))
                            config->MakeDepthCopy = makeDepthCopy;
                        ShowHelpMarker("复制深度缓冲区\n可修复 AMD GPU 在 Windows 下某些游戏中的画面损坏"
                                       "GPU\n可能导致卡顿，因此最好仅在必要时使用");
                    }
                    else if (state.swapchainApi == Vulkan)
                    {
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                           "此菜单可见时 DLSSG 会被有意禁用");
                        ImGui::Spacing();
                    }

                    if (DLSSGMod::isLoaded())
                    {
                        if (DLSSGMod::is120orNewer())
                        {
                            if (ImGui::Checkbox("启用调试视图", &state.DLSSGDebugView))
                            {
                                DLSSGMod::setDebugView(state.DLSSGDebugView);
                            }
                            if (ImGui::Checkbox("仅插值帧", &state.DLSSGInterpolatedOnly))
                            {
                                DLSSGMod::setInterpolatedOnly(state.DLSSGInterpolatedOnly);
                            }
                        }
                        else if (DLSSGMod::FSRDebugView() != nullptr)
                        {
                            if (ImGui::Checkbox("启用调试视图", &state.DLSSGDebugView))
                            {
                                DLSSGMod::FSRDebugView()(state.DLSSGDebugView);
                            }
                        }
                    }
                }

                // FSR-FG Inputs
                if (state.currentFGSwapchain != nullptr && state.workingMode != WorkingMode::Nvngx &&
                    (state.activeFgInput == FGInput::FSRFG || state.activeFgInput == FGInput::FSRFG30))
                {
                    SeparatorWithHelpMarker("帧生成（FSR-FG 输入）", "请在游戏内选择 FSR-FG");

                    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);
                    if (fgOutput != nullptr)
                    {
                        ImGui::Text("当前 FSR-FG 状态：");
                        ImGui::SameLine();
                        if (state.FSRFGInputActive)
                        {
                            if (fgOutput->IsActive())
                                ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                            else
                                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "启用帧生成");
                        }
                        else
                        {
                            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
                            ImGui::Text("请在游戏选项中启用 FSR 帧生成\n"
                                        "您可能需要先选择 FSR");
                        }
                    }

                    bool skipConfig = config->FSRFGSkipConfigForHudless.value_or_default();
                    if (ImGui::Checkbox("为 HUDless 跳过配置", &skipConfig))
                        config->FSRFGSkipConfigForHudless = skipConfig;

                    ShowHelpMarker("不使用 ffxConfig 中设置的 HUDless");

                    ImGui::SameLine(0.0f, 6.0f);

                    bool skipDispatch = config->FSRFGSkipDispatchForHudless.value_or_default();
                    if (ImGui::Checkbox("为 HUDless 跳过 Dispatch", &skipDispatch))
                        config->FSRFGSkipDispatchForHudless = skipDispatch;

                    ShowHelpMarker("不使用 ffxDispatch 中设置的 HUDless");
                }

                // Streamline FG Inputs
                if (state.currentFGSwapchain != nullptr && state.workingMode != WorkingMode::Nvngx &&
                    state.activeFgInput == FGInput::DLSSG)
                {
                    SeparatorWithHelpMarker("帧生成（Streamline FG 输入）", "请在游戏内选择 DLSS-FG");

                    auto fgOutput = reinterpret_cast<IFGFeature_Dx12*>(state.currentFG);

                    if (!ReflexHooks::isReflexHooked())
                    {
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "Reflex 未挂钩");
                        ImGui::Text("如果您使用的是 AMD/Intel 显卡，请确保已安装 fakenvapi");
                    }
                    else if (fgOutput != nullptr)
                    {
                        ImGui::Text("当前 Streamline FG 状态：");
                        ImGui::SameLine();
                        if ((state.FGLastFrame - state.DLSSGLastFrame) < 3)
                        {
                            if (fgOutput->IsActive())
                                ImGui::TextColored(toneMapColor(ImVec4(0.f, 1.f, 0.25f, 1.f)), "开");
                            else
                                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 0.647f, 0.0f, 1.f)), "启用帧生成");
                        }
                        else
                        {
                            ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)), "关");
                            ImGui::Text("请在游戏选项中启用 DLSS 帧生成\n"
                                        "您可能需要先选择 DLSS");
                        }
                    }
                }

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    // FSR Common -----------------
                    if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
                        (state.activeFgOutput == FGOutput::FSRFG || currentBackend.rfind("fsr", 0) == 0))
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

                                ShowHelpMarker("可能有助于获得更好的画质");
                            }
                            else
                            {
                                if (ImGui::SliderFloat("水平视场角", &hfov, 0.0f, 180.0f, "%.1f"))
                                    config->FsrHorizontalFov = hfov;

                                ShowHelpMarker("可能有助于获得更好的画质");
                            }

                            float cameraNear;
                            float cameraFar;

                            cameraNear = config->FsrCameraNear.value_or_default();
                            cameraFar = config->FsrCameraFar.value_or_default();

                            if (ImGui::SliderFloat("相机近平面", &cameraNear, 0.1f, 500000.0f, "%.1f"))
                                config->FsrCameraNear = cameraNear;
                            ShowHelpMarker("可能有助于获得更好的画质\n"
                                           "并可能减少拖影");

                            if (ImGui::SliderFloat("相机远平面", &cameraFar, 0.1f, 500000.0f, "%.1f"))
                                config->FsrCameraFar = cameraFar;
                            ShowHelpMarker("可能有助于获得更好的画质\n"
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

                // Framerate ---------------------
                if (state.reflexLimitsFps || config->OverlayMenu)
                {
                    SeparatorWithHelpMarker(
                        "帧率",
                        "尽可能使用 Reflex\n在 AMD/Intel 显卡上，可使用 Fakenvapi 替代 Reflex");

                    static std::string currentMethod {};
                    if (state.reflexLimitsFps)
                    {
                        if (fakenvapi::updateModeAndContext())
                        {
                            auto mode = fakenvapi::getCurrentMode();

                            if (mode == Mode::AntiLag2)
                                currentMethod = "AntiLag 2";
                            else if (mode == Mode::LatencyFlex)
                                currentMethod = "LatencyFlex";
                            else if (mode == Mode::XeLL)
                                currentMethod = "XeLL";
                            else if (mode == Mode::AntiLagVk)
                                currentMethod = "Vulkan AntiLag";

                            if (state.rtssReflexInjection && mode == Mode::AntiLag2 &&
                                config->FGOutput == FGOutput::FSRFG)
                                ImGui::TextColored(
                                    toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)),
                                    "将 RTSS Reflex 注入与 AntiLag 2 和 FSR FG 一起使用可能会导致问题");
                        }
                        else
                        {
                            currentMethod = "Reflex";
                        }
                    }
                    else
                    {
                        currentMethod = "回退";
                    }

                    if (state.rtssReflexInjection)
                        currentMethod.append(" (RTSS)");

                    ImGui::Text("当前方式：%s", currentMethod.c_str());

                    if (state.reflexShowWarning)
                    {
                        ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                                           "将 Reflex 的限制与 OptiFG 一起使用会有性能开销");

                        ImGui::Spacing();
                    }

                    // set initial value
                    if (std::isinf(_limitFps))
                        _limitFps = config->FramerateLimit.value_or_default();

                    ImGui::SliderFloat("FPS 上限", &_limitFps, 0, 200, "%.0f");

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
                        auto fpsLimitTech = fakenvapi::getCurrentMode();
                        constexpr float margin = 0.3f; // in ms
                        float frameCap = std::round(10000.f / (1000.f / refreshRateF + margin)) / 10.f;

                        if (fpsLimitTech == Mode::AntiLag2 || fpsLimitTech == Mode::AntiLagVk)
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

                // FAKENVAPI ---------------------------
                if (fakenvapi::isUsingFakenvapi())
                {
                    ImGui::SeparatorText("fakenvapi");

                    if (bool logs = config->FN_EnableLogs.value_or_default();
                        ImGui::Checkbox("启用日志写入文件", &logs))
                        config->FN_EnableLogs = logs;

                    ImGui::BeginDisabled(!config->FN_EnableLogs.value_or_default());

                    ImGui::SameLine(0.0f, 6.0f);
                    if (bool traceLogs = config->FN_EnableTraceLogs.value_or_default();
                        ImGui::Checkbox("启用跟踪日志", &traceLogs))
                        config->FN_EnableTraceLogs = traceLogs;

                    ImGui::EndDisabled();

                    if (bool forceLFX = config->FN_ForceLatencyFlex.value_or_default();
                        ImGui::Checkbox("强制 LatencyFlex", &forceLFX))
                        config->FN_ForceLatencyFlex = forceLFX;
                    ShowHelpMarker("默认情况下，可用时使用 AntiLag 2/XeLL。\n"
                                   "此设置可让您强制改用 LatencyFlex");

                    // clang-format off
                    static const std::vector<MenuOption<uint32_t>> lfx_modes = {
                        { 0, "保守",
                            "最安全，但可能无法很好地降低延迟" },
                        { 1, "激进",
                            "改善延迟，但在某些情况下 FPS 下降可能超出预期" },
                        { 2, "Reflex ID",
                            "可用时效果最佳，部分游戏不兼容（例如《赛博朋克》)\n"
                            "并将回退到激进模式" }
                    };

                    PopulateCombo("LatencyFlex 模式", config->FN_LatencyFlexMode, lfx_modes);

                    static const std::vector<MenuOption<uint32_t>> reflex_modes = { { 0, "跟随游戏设置" },
                                                                                    { 1, "强制禁用" },
                                                                                    { 2, "强制启用" } };

                    PopulateCombo("强制 Reflex", config->FN_ForceReflex, reflex_modes);
                    // clang-format on

                    if (ImGui::Button("应用##2"))
                    {
                        config->SaveFakenvapiIni();
                    }
                }

                // NEXT COLUMN -----------------
                ImGui::TableNextColumn();

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    // SHARPNESS -----------------------------
                    ImGui::SeparatorText("锐化");

                    if (bool overrideSharpness = config->OverrideSharpness.value_or_default();
                        ImGui::Checkbox("覆盖", &overrideSharpness))
                    {
                        config->OverrideSharpness = overrideSharpness;

                        if (currentBackend == "dlss" && currentFeature->Version().major < 3)
                        {
                            state.newBackend = currentBackend;
                            MARK_ALL_BACKENDS_CHANGED();
                        }
                    }
                    ShowHelpMarker("忽略游戏发送的值\n"
                                   "并使用下方设置的值");

                    ImGui::BeginDisabled(!config->OverrideSharpness.value_or_default());

                    float sharpness = config->Sharpness.value_or_default();

                    if (ImGui::SliderFloat("锐化", &sharpness, 0.0f, 1.0f))
                        config->Sharpness = sharpness;

                    ImGui::EndDisabled();

                    // RCAS
                    // if (state.api == DX12 || state.api == DX11)
                    {
                        // xess or dlss version >= 2.5.1
                        constexpr feature_version requiredDlssVersion = { 2, 5, 1 };
                        rcasEnabled = (currentBackend == "xess" ||
                                       (currentBackend == "dlss" && currentFeature->Version() >= requiredDlssVersion));

                        ImGui::Spacing();
                        ImGui::Spacing();

                        if (bool rcas = config->RcasEnabled.value_or(rcasEnabled);
                            ImGui::Checkbox("启用 RCAS/DA", &rcas))
                            config->RcasEnabled = rcas;

                        ShowHelpMarker("启用 OptiScaler 的锐化滤镜\n"
                                       "默认使用游戏提供的锐化值\n"
                                       "在“锐化”下选择“覆盖”并调整滑块\n"
                                       "以更改它\n\n"
                                       "某些超分器自带锐化滤镜，因此此\n"
                                       "选项并非总是需要");

                        ImGui::BeginDisabled(!config->RcasEnabled.value_or(rcasEnabled));

                        bool useDA = Config::Instance()->UseDepthAwareSharpen.value_or_default();
                        bool useLCDA = Config::Instance()->UseDASDepthAwareSharpen.value_or_default();
                        bool useRcas = !useDA && !useLCDA;

                        if (ImGui::Checkbox("RCAS", &useRcas) && useRcas)
                        {
                            Config::Instance()->UseDepthAwareSharpen = !useRcas;
                            Config::Instance()->UseDASDepthAwareSharpen = !useRcas;
                        }

                        ShowHelpMarker("使用 AMD 的 RCAS\n"
                                       "已修改以添加对比度参数\n"
                                       "并支持 MAS");

                        ImGui::SameLine(0.0f, 6.0f);

                        if (ImGui::Checkbox("深度感知 (RCAS)", &useDA) && useDA)
                        {
                            Config::Instance()->UseDepthAwareSharpen = useDA;
                            Config::Instance()->UseDASDepthAwareSharpen = !useDA;
                        }

                        ShowHelpMarker("使用深度感知锐化（RCAS）\n"
                                       "更智能的锐化，伪影更少，\n"
                                       "但负担更重\n\n"
                                       "物体越远，\n"
                                       "应用的锐化越多");

                        ImGui::SameLine(0.0f, 6.0f);

                        if (ImGui::Checkbox("深度感知 (DAS)", &useLCDA) && useLCDA)
                        {
                            Config::Instance()->UseDASDepthAwareSharpen = useLCDA;
                            Config::Instance()->UseDepthAwareSharpen = !useLCDA;
                        }

                        ShowHelpMarker("使用深度感知锐化（DAS）\n"
                                       "深度感知的方向性自适应亮度锐化器\n"
                                       "更智能的锐化，伪影更少，\n"
                                       "但负担更重\n\n"
                                       "物体越远，\n"
                                       "应用的锐化越多");

                        ImGui::Spacing();

                        if (bool overrideMotionSharpness = config->MotionSharpnessEnabled.value_or_default();
                            ImGui::Checkbox("启用运动自适应锐化", &overrideMotionSharpness))
                            config->MotionSharpnessEnabled = overrideMotionSharpness;
                        ShowHelpMarker("根据运动启用锐化调整");

                        if (useDA || useLCDA)
                        {
                            bool depthLinear = config->DADepthIsLinear.value_or_default();
                            if (ImGui::Checkbox("线性深度", &depthLinear))
                            {
                                if (depthLinear)
                                    config->DADepthIsLinear = true;
                                else
                                    config->DADepthIsLinear.reset();
                            }

                            ShowHelpMarker("大多数游戏使用非线性深度，但\n"
                                           "DLSS-D 可能需要启用此选项。\n"
                                           "可通过调试视图验证");

                            ImGui::SameLine(0.0f, 6.0f);

                            if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                                ImGui::Checkbox("深度感知调试", &overrideMSDebug))
                                config->MotionSharpnessDebug = overrideMSDebug;

                            ShowHelpMarker("启用 DAS 调试视图\n\n"
                                           "检测到的边缘显示蓝色色调");

                            if (auto ch = ScopedCollapsingHeader("高级深度感知参数"); ch.IsHeaderOpen())
                            {
                                ScopedIndent indent {};
                                ImGui::Spacing();

                                if (bool clamp = config->DAClampOutput.value_or(false);
                                    ImGui::Checkbox("钳制输出", &clamp))
                                {
                                    if (clamp)
                                        config->DAClampOutput = true;
                                    else
                                        config->DAClampOutput.reset();
                                }

                                ShowHelpMarker(
                                    "将最终图像钳制到 [0, 1] 范围。\n\n"
                                    "防止过冲伪影，如明亮光晕或负色。\n"
                                    "推荐用于 LDR 管线；HDR 可依据色调映射选择开启。\n\n"
                                    "未设置时，OptiScaler 通过超分器的 HDR 标志控制");

                                if (depthLinear)
                                {
                                    float depthBias = config->DADepthBias.value_or(0.0015f);
                                    if (ImGui::SliderFloat("深度偏差", &depthBias, 0.005f, 0.03f, "%.4f"))
                                        config->DADepthBias = depthBias;

                                    ShowHelpMarker(
                                        "在边缘检测前忽略微小的深度差异。\n\n"
                                        "较高的值减少轻微深度变化引起的闪烁和噪点，但可能"
                                        "软化真实的几何边缘。\n"
                                        "较低的值保留精细细节，但可能导致边缘"
                                        "检测不稳定或嘈杂。");

                                    float depthScale = config->DADepthScale.value_or(250.0f);
                                    if (ImGui::SliderFloat("深度缩放", &depthScale, 100.0f, 600.0f, "%.1f"))
                                        config->DADepthScale = depthScale;

                                    ShowHelpMarker(
                                        "控制锐化在深度边缘处被减弱的强度。\n\n"
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

                                    ShowHelpMarker(
                                        "在边缘检测前忽略微小的深度差异。\n\n"
                                        "较高的值减少轻微深度变化引起的闪烁和噪点，但可能"
                                        "软化真实的几何边缘。\n"
                                        "较低的值保留精细细节，但可能导致边缘"
                                        "检测不稳定或嘈杂。");

                                    float depthScale = config->DADepthScale.value_or(35.0f);
                                    if (ImGui::SliderFloat("深度缩放", &depthScale, 25.0f, 400.0f, "%.1f"))
                                        config->DADepthScale = depthScale;

                                    ShowHelpMarker(
                                        "控制锐化在深度边缘处被减弱的强度。\n\n"
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

                            ShowHelpMarker("控制高对比度区域的锐化。");

                            ImGui::BeginDisabled(!config->ContrastEnabled.value_or_default());

                            float contrast = config->Contrast.value_or_default();
                            if (ImGui::SliderFloat("对比度", &contrast, -2.0f, 2.0f, "%.2f"))
                                config->Contrast = contrast;

                            ShowHelpMarker("正值降低高对比度区域的锐化。\n"
                                           "负值增加高对比度区域的锐化。");

                            ImGui::EndDisabled();
                        }

                        ImGui::Spacing();
                        if (auto ch = ScopedCollapsingHeader("运动自适应锐化##2"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            ImGui::BeginDisabled(!config->MotionSharpnessEnabled.value_or_default());

                            if (!useDA & !useLCDA)
                            {
                                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                                    ImGui::Checkbox("MAS 调试", &overrideMSDebug))
                                    config->MotionSharpnessDebug = overrideMSDebug;
                                ShowHelpMarker("越红的区域将应用更多锐化\n"
                                               "绿色区域将减少锐化");
                            }

                            float motionSharpness = config->MotionSharpness.value_or_default();
                            ImGui::SliderFloat("运动锐化强度", &motionSharpness, -1.0f, 1.0f, "%.3f");
                            config->MotionSharpness = motionSharpness;

                            ShowHelpMarker("运动可增加或减少的最大锐化量。\n\n"
                                           "负值减少运动中的锐化（推荐）。\n"
                                           "正值增加运动中的锐化。\n\n"
                                           "最终调整随运动缩放，并以该值为上限。");

                            float motionThreshod = config->MotionThreshold.value_or_default();
                            ImGui::SliderFloat("运动阈值", &motionThreshod, 0.0f, 100.0f, "%.2f");
                            config->MotionThreshold = motionThreshod;

                            ShowHelpMarker(
                                "开始基于运动的锐化调整所需的最小运动量。\n\n"
                                "较高的值忽略微小运动（更稳定）。\n"
                                "较低的值对细微运动更敏感。");

                            float motionScale = config->MotionScaleLimit.value_or_default();
                            ImGui::SliderFloat("运动范围", &motionScale, 0.01f, 100.0f, "%.2f");
                            config->MotionScaleLimit = motionScale;

                            ShowHelpMarker(
                                "定义效果从零到全强度的运动范围。\n\n"
                                "高于阈值的值映射到此范围。\n"
                                "较大的值使响应更平滑、更渐进。\n"
                                "较小的值使效果反应更快、更激进。");

                            ImGui::EndDisabled();

                            ImGui::Spacing();
                            ImGui::Spacing();
                        }

                        ImGui::EndDisabled();
                    }

                    // UPSCALE RATIO OVERRIDE -----------------

                    auto minSliderLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
                    auto maxSliderLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;

                    ImGui::SeparatorText("超分倍率覆盖");

                    if (bool upOverride = config->UpscaleRatioOverrideEnabled.value_or_default();
                        ImGui::Checkbox("覆盖全部", &upOverride))
                    {
                        config->UpscaleRatioOverrideEnabled = upOverride;

                        if (upOverride)
                            config->QualityRatioOverrideEnabled = false;
                    }
                    ShowHelpMarker("用设置的值覆盖所有超分器预设\n\n"
                                   "在 1080p 屏幕上 1.5x 意味着内部分辨率为 720p\n"
                                   "1080 / 1.5 = 720");

                    if (bool qOverride = config->QualityRatioOverrideEnabled.value_or_default();
                        ImGui::Checkbox("按画质预设分别覆盖", &qOverride))
                    {
                        config->QualityRatioOverrideEnabled = qOverride;

                        if (qOverride)
                            config->UpscaleRatioOverrideEnabled = false;
                    }

                    ShowHelpMarker("允许您单独覆盖每个预设的倍率\n"
                                   "请注意，并非每个游戏都支持所有画质预设\n\n"
                                   "在 1080p 屏幕上 1.5x 意味着内部分辨率为 720p\n"
                                   "1080 / 1.5 = 720");

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

                    if (currentFeature != nullptr && !currentFeature->IsFrozen())
                    {
                        // OUTPUT SCALING -----------------------------
                        // if (state.api == DX12 || state.api == DX11)
                        {
                            // if motion vectors are not display size
                            ImGui::BeginDisabled(!currentFeature->LowResMV() &&
                                                 currentFeature->RenderWidth() != currentFeature->DisplayWidth());

                            ImGui::SeparatorText("输出缩放");

                            float defaultRatio = 1.5f;

                            if (_ssRatio == 0.0f)
                            {
                                _ssRatio = config->OutputScalingMultiplier.value_or(defaultRatio);
                                _ssEnabled = config->OutputScalingEnabled.value_or_default();
                                _ssDownsampler = config->OutputScalingDownscaler.value_or_default();
                            }

                            ImGui::BeginDisabled((currentBackend == "xess" || currentBackend == "dlss") &&
                                                 currentFeature->RenderWidth() > currentFeature->DisplayWidth());
                            ImGui::Checkbox("启用", &_ssEnabled);
                            ImGui::EndDisabled();

                            ShowHelpMarker("在内部将图像放大到更高的输出分辨率\n"
                                           "然后将其缩小回您的显示分辨率\n\n"
                                           "值 <1.0 使超分器负载更低\n"
                                           "值 >1.0 以性能为代价使图像更锐利\n\n"
                                           "如果呈灰色，请查看 Git Wiki - 虚幻引擎调整\n\n"
                                           "底部显示目标分辨率和总倍率（最大总计 3.0！）");

                            ImGui::SameLine(0.0f, 6.0f);

                            ImGui::BeginDisabled(!_ssEnabled);
                            {
                                ImGui::PushItemWidth(95.0f * menuResScale);

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
                                        "Lanczos2 的重型版本。\n图像最锐利，但最容易产生振铃。" },
                                    { Scaler::Kaiser2, "Kaiser2",
                                        "与 Lanczos2 类似。\n比 Lanczos 更平滑、伪影更少，但略微模糊。" },
                                    { Scaler::Kaiser3, "Kaiser3",
                                        "与 Lanczos3 类似。\n伪影远少于 Lanczos3，但对 GPU 负担大得多。" },
                                    { Scaler::Magic, "MAGIC",
                                        "专为防止伪影而设计。\n消除生硬光晕以获得自然观感，但可能略显柔和。" }
                                };
                                // clang-format on

                                const bool isUpsampleRatio = _ssRatio < 1.0f;
                                const std::string disabledReason =
                                    "当倍率低于 1.0 时，仅支持 FSR1 和 Bicubic。";

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

                                if (currentFeature->Name() == "DLSSD")
                                    state.newBackend = "dlssd";
                                else
                                    state.newBackend = currentBackend;

                                MARK_ALL_BACKENDS_CHANGED();
                            }
                            ImGui::EndDisabled();

                            ImGui::BeginDisabled(!_ssEnabled ||
                                                 currentFeature->RenderWidth() > currentFeature->DisplayWidth());
                            ImGui::SliderFloat("倍率", &_ssRatio, 0.5f, 3.0f, "%.2f");
                            ImGui::EndDisabled();

                            if (currentFeature != nullptr && !currentFeature->IsFrozen())
                            {
                                ImGui::Text("输出缩放：%s，目标分辨率：%dx%d（%.2f）\n抖动采样数：%d",
                                            config->OutputScalingEnabled.value_or_default() ? "已启用" : "已禁用",
                                            (uint32_t) (currentFeature->DisplayWidth() * _ssRatio),
                                            (uint32_t) (currentFeature->DisplayHeight() * _ssRatio),
                                            ((float) currentFeature->DisplayWidth() * _ssRatio) /
                                                (float) currentFeature->RenderWidth(),
                                            currentFeature->JitterCount());
                            }

                            ImGui::EndDisabled();
                        }
                    }

                    // INIT -----------------------------
                    ImGui::SeparatorText("初始化标志");
                    if (ImGui::BeginTable("init", 2, ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableNextColumn();

                        // AutoExposure is always enabled for XeSS with native Dx11
                        bool autoExposureDisabled = state.api == API::DX11 && currentBackend == "xess";
                        ImGui::BeginDisabled(autoExposureDisabled);

                        if (bool autoExposure = currentFeature->AutoExposure();
                            ImGui::Checkbox("自动曝光", &autoExposure))
                        {
                            config->AutoExposure = autoExposure;
                            ReInitUpscaler();
                        }
                        ShowResetButton(&config->AutoExposure, "R");
                        ShowHelpMarker("某些虚幻引擎游戏需要此项\n\n"
                                       "如果颜色闪烁或\n"
                                       "物体有拖影，请尝试");

                        ImGui::EndDisabled();

                        ImGui::TableNextColumn();
                        auto accessToReactiveMask = currentFeature->AccessToReactiveMask();
                        ImGui::BeginDisabled(!accessToReactiveMask);

                        bool canUseReactiveMask =
                            accessToReactiveMask && currentBackend != "dlss" &&
                            (currentBackend != "xess" || currentFeature->Version() >= feature_version { 2, 0, 1 });

                        bool disableReactiveMask = config->DisableReactiveMask.value_or(!canUseReactiveMask);

                        if (ImGui::Checkbox("禁用反应蒙版", &disableReactiveMask))
                        {
                            config->DisableReactiveMask = disableReactiveMask;

                            if (currentBackend == "xess")
                            {
                                state.newBackend = currentBackend;
                                MARK_ALL_BACKENDS_CHANGED();
                            }
                        }

                        ImGui::EndDisabled();

                        if (accessToReactiveMask)
                            ShowHelpMarker("允许使用反应蒙版\n"
                                           "请注意，发送给 DLSS 的反应蒙版\n"
                                           "与 FSR/XeSS 结合使用时不会产生好的图像");
                        else
                            ShowHelpMarker("此选项已禁用，因为游戏不提供反应蒙版");

                        ImGui::EndTable();

                        ImGui::Spacing();
                        if (auto ch = ScopedCollapsingHeader("高级初始化标志"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            if (ImGui::BeginTable("init2", 2, ImGuiTableFlags_SizingStretchProp))
                            {
                                ImGui::TableNextColumn();
                                if (bool depth = currentFeature->DepthInverted();
                                    ImGui::Checkbox("深度反转", &depth))
                                {
                                    config->DepthInverted = depth;
                                    ReInitUpscaler();
                                }
                                ShowResetButton(&config->DepthInverted, "重置##2");
                                ShowHelpMarker("您通常不需要更改它");

                                ImGui::TableNextColumn();
                                if (bool hdr = currentFeature->IsHdr(); ImGui::Checkbox("HDR", &hdr))
                                {
                                    config->HDR = hdr;
                                    ReInitUpscaler();
                                }
                                ShowResetButton(&config->HDR, "重置##1");
                                ShowHelpMarker("可能有助于解决部分游戏中的紫色色调问题");

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
                                ShowResetButton(&config->DisplayResolution, "重置##4");
                                ShowHelpMarker("主要用于修复虚幻引擎游戏\n"
                                               "屏幕左上部分会变模糊");

                                ImGui::TableNextColumn();

                                if (bool jitter = currentFeature->JitteredMV();
                                    ImGui::Checkbox("抖动消除", &jitter))
                                {
                                    config->JitterCancellation = jitter;
                                    ReInitUpscaler();
                                }
                                ShowResetButton(&config->JitterCancellation, "重置##3");
                                ShowHelpMarker("修复游戏发送已预应用抖动的运动数据的问题");

                                ImGui::TableNextColumn();
                                ImGui::EndTable();
                            }

                            if (currentFeature->AccessToReactiveMask() && currentBackend != "dlss")
                            {
                                ImGui::BeginDisabled(config->DisableReactiveMask.value_or(currentBackend == "xess"));

                                bool binaryMask = state.api == Vulkan || currentBackend == "xess";
                                auto defaultBias = binaryMask ? 0.0f : 0.45f;
                                auto maskBias = config->DlssReactiveMaskBias.value_or(defaultBias);

                                if (!binaryMask)
                                {
                                    if (ImGui::SliderFloat("反应蒙版偏移", &maskBias, 0.0f, 0.9f, "%.2f"))
                                        config->DlssReactiveMaskBias = maskBias;

                                    ShowHelpMarker("高于 0 的值将启用反应蒙版的使用");
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
                    }
                }

                // QUIRKS -----------------------------
                if (state.detectedQuirks.size() > 0)
                {
                    ImGui::Spacing();
                    if (auto ch = ScopedCollapsingHeader("生效的兼容性修正"); ch.IsHeaderOpen())
                    {
                        ScopedIndent indent {};
                        ImGui::Spacing();

                        for (const auto& quirk : state.detectedQuirks)
                        {
                            ImGui::TextWrapped("%s", quirk.c_str());
                        }
                    }
                }

                // ADVANCED SETTINGS -----------------------------
                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("高级设置"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (currentFeature != nullptr && !currentFeature->IsFrozen())
                    {
                        bool extendedLimits = config->ExtendedLimits.value_or_default();
                        if (ImGui::Checkbox("启用扩展限制", &extendedLimits))
                            config->ExtendedLimits = extendedLimits;

                        ShowHelpMarker("扩展画质预设的滑块限制\n\n"
                                       "使用此选项会更改分辨率检测逻辑\n"
                                       "并可能导致问题和崩溃！");
                    }

                    bool pcShaders = config->UsePrecompiledShaders.value_or_default();
                    if (ImGui::Checkbox("使用预编译着色器", &pcShaders))
                    {
                        config->UsePrecompiledShaders = pcShaders;
                        state.newBackend = currentBackend;
                        MARK_ALL_BACKENDS_CHANGED();
                    }

                    // DRS
                    ImGui::SeparatorText("DRS（动态分辨率缩放）");
                    if (ImGui::BeginTable("drs", 2, ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableNextColumn();
                        if (bool drsMin = config->DrsMinOverrideEnabled.value_or_default();
                            ImGui::Checkbox("覆盖最小值", &drsMin))
                            config->DrsMinOverrideEnabled = drsMin;
                        ShowHelpMarker("修复游戏忽略官方 DRS 限制的问题");

                        ImGui::TableNextColumn();
                        if (bool drsMax = config->DrsMaxOverrideEnabled.value_or_default();
                            ImGui::Checkbox("覆盖最大值", &drsMax))
                            config->DrsMaxOverrideEnabled = drsMax;
                        ShowHelpMarker("修复游戏忽略官方 DRS 限制的问题");

                        ImGui::EndTable();
                    }

                    // Non-DLSS hotfixes -----------------------------
                    if (currentFeature != nullptr && !currentFeature->IsFrozen() && currentBackend != "dlss")
                    {
                        // BARRIERS -----------------------------
                        ImGui::Spacing();
                        if (auto ch = ScopedCollapsingHeader("资源屏障"); ch.IsHeaderOpen())
                        {
                            ScopedIndent indent {};
                            ImGui::Spacing();

                            AddResourceBarrier("颜色", &config->ColorResourceBarrier);
                            AddResourceBarrier("深度", &config->DepthResourceBarrier);
                            AddResourceBarrier("运动", &config->MVResourceBarrier);
                            AddResourceBarrier("曝光", &config->ExposureResourceBarrier);
                            AddResourceBarrier("蒙版", &config->MaskResourceBarrier);
                            AddResourceBarrier("输出", &config->OutputResourceBarrier);
                        }

                        // HOTFIXES -----------------------------
                        if (state.api == DX12)
                        {
                            ImGui::Spacing();
                            if (auto ch = ScopedCollapsingHeader("根签名"); ch.IsHeaderOpen())
                            {
                                ScopedIndent indent {};
                                ImGui::Spacing();

                                if (bool crs = config->RestoreComputeSignature.value_or_default();
                                    ImGui::Checkbox("恢复计算根签名", &crs))
                                    config->RestoreComputeSignature = crs;

                                if (bool grs = config->RestoreGraphicSignature.value_or_default();
                                    ImGui::Checkbox("恢复图形根签名", &grs))
                                    config->RestoreGraphicSignature = grs;
                            }
                        }
                    }
                }

                // LOGGING -----------------------------
                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("日志"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (config->LogToConsole.value_or_default() || config->LogToFile.value_or_default() ||
                        config->LogToNGX.value_or_default())
                        spdlog::default_logger()->set_level(
                            (spdlog::level::level_enum) config->LogLevel.value_or_default());
                    else
                        spdlog::default_logger()->set_level(spdlog::level::off);

                    if (bool toFile = config->LogToFile.value_or_default(); ImGui::Checkbox("写入文件", &toFile))
                    {
                        config->LogToFile = toFile;
                        PrepareLogger();
                    }

                    ImGui::SameLine(0.0f, 6.0f);
                    if (bool toConsole = config->LogToConsole.value_or_default();
                        ImGui::Checkbox("输出到控制台", &toConsole))
                    {
                        config->LogToConsole = toConsole;
                        PrepareLogger();
                    }

                    const char* logLevels[] = { "跟踪", "调试", "信息", "警告", "错误" };
                    const char* selectedLevel = logLevels[config->LogLevel.value_or_default()];

                    if (ImGui::BeginCombo("日志级别", selectedLevel))
                    {
                        for (int n = 0; n < 5; n++)
                        {
                            if (ImGui::Selectable(logLevels[n], (config->LogLevel.value_or_default() == n)))
                            {
                                config->LogLevel = n;
                                spdlog::default_logger()->set_level(
                                    (spdlog::level::level_enum) config->LogLevel.value_or_default());
                            }
                        }

                        ImGui::EndCombo();
                    }
                }

                // THEME -----------------------------
                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("菜单主题与颜色"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    bool lightTheme = config->LightTheme.value_or_default();

                    const ImVec4 bgDark =
                        lightTheme ? ImVec4(0.80f, 0.82f, 0.86f, 1.00f) : ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
                    const ImVec4 bgMid =
                        lightTheme ? ImVec4(0.89f, 0.91f, 0.95f, 1.00f) : ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
                    const ImVec4 bgLight =
                        lightTheme ? ImVec4(0.96f, 0.97f, 0.99f, 1.00f) : ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

                    auto Mix = [](const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
                    { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };

                    auto AccentSoft = [&](ImVec4 accent, float alpha = 1.0f)
                    {
                        return toneMapColor(lightTheme ? Mix(bgLight, accent, 0.24f, alpha)
                                                       : Mix(bgDark, accent, 0.32f, alpha));
                    };

                    auto AccentMed = [&](ImVec4 accent, float alpha = 1.0f)
                    {
                        return toneMapColor(lightTheme ? Mix(bgLight, accent, 0.42f, alpha)
                                                       : Mix(bgDark, accent, 0.55f, alpha));
                    };

                    auto AccentStrong = [&](ImVec4 accent, float alpha = 1.0f)
                    { return toneMapColor(ImVec4(accent.x, accent.y, accent.z, alpha)); };

                    if (ImGui::Checkbox("浅色主题", &lightTheme))
                    {
                        config->LightTheme = lightTheme;
                        ApplyThemeStyle();
                    }

                    ImGui::SeparatorText("强调色");

                    ImGui::Text("预设：");
                    ImGui::SameLine(0.0f, 6.0f);

                    ImVec4 colorBlue = { 0.00f, 0.40f, 0.77f, 1.0f };
                    ImVec4 colorTeal = { 0.00f, 1.00f, 0.91f, 1.0f };
                    ImVec4 colorGray = { 0.54f, 0.54f, 0.54f, 1.0f };
                    ImVec4 colorYellow = { 1.00f, 0.89f, 0.00f, 1.0f };
                    ImVec4 colorGreen = { 0.25f, 1.00f, 0.00f, 1.0f };
                    ImVec4 colorRed = { 1.00f, 0.00f, 0.00f, 1.0f };
                    ImVec4 colorOrange = { 1.00f, 0.52f, 0.00f, 1.0f };
                    ImVec4 colorPurple = { 0.576f, 0.00f, 1.00f, 1.0f };

                    ImVec4 color = {};

                    color = colorBlue;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("蓝色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorTeal;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("青色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorGray;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("灰色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorYellow;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("黄色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorGreen;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("绿色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorRed;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("红色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorOrange;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("橙色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorPurple;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("紫色"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuAccentColorR = color.x;
                        config->MenuAccentColorG = color.y;
                        config->MenuAccentColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

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

                    ImGui::SeparatorText("背景色");

                    ImGui::Text("预设：");
                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorBlue;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("蓝色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorTeal;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("青色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorGray;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("灰色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorYellow;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("黄色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorGreen;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("绿色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorRed;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("红色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorOrange;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("橙色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    ImGui::SameLine(0.0f, 6.0f);

                    color = colorPurple;
                    ImGui::PushStyleColor(ImGuiCol_Button, AccentSoft(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, AccentMed(color));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, AccentStrong(color));

                    if (ImGui::Button("紫色##2"))
                    {
                        ImGui::PopStyleColor(3);

                        config->MenuBGColorR = color.x;
                        config->MenuBGColorG = color.y;
                        config->MenuBGColorB = color.z;
                        ApplyThemeStyle();
                    }
                    else
                    {
                        ImGui::PopStyleColor(3);
                    }

                    float bgColor[3] = { config->MenuBGColorR.value_or_default(),
                                         config->MenuBGColorG.value_or_default(),
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

                // FPS OVERLAY -----------------------------
                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("FPS 覆盖层"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    bool fpsEnabled = config->ShowFps.value_or_default();
                    if (ImGui::Checkbox("启用 FPS 覆盖层", &fpsEnabled))
                        config->ShowFps = fpsEnabled;

                    ImGui::SameLine(0.0f, 6.0f);

                    bool fpsHorizontal = config->FpsOverlayHorizontal.value_or_default();
                    if (ImGui::Checkbox("水平", &fpsHorizontal))
                        config->FpsOverlayHorizontal = fpsHorizontal;

                    const char* fpsPosition[] = { "左上", "右上", "左下", "右下" };
                    const char* selectedPosition = fpsPosition[config->FpsOverlayPos.value_or_default()];

                    if (ImGui::BeginCombo("覆盖层位置", selectedPosition))
                    {
                        for (int n = 0; n < 4; n++)
                        {
                            if (ImGui::Selectable(fpsPosition[n], (config->FpsOverlayPos.value_or_default() == n)))
                                config->FpsOverlayPos = n;
                        }

                        ImGui::EndCombo();
                    }

                    const char* fpsType[] = { "仅 FPS", "简洁",       "详细",      "详细 + 图表",
                                              "完整",     "完整 + 图表", "Reflex 时序" };
                    const char* selectedType = fpsType[config->FpsOverlayType.value_or_default()];

                    if (ImGui::BeginCombo("覆盖层类型", selectedType))
                    {
                        for (int n = 0; n < std::size(fpsType); n++)
                        {
                            if (ImGui::Selectable(fpsType[n], (config->FpsOverlayType.value_or_default() == n)))
                                config->FpsOverlayType = (FpsOverlay) n;
                        }

                        ImGui::EndCombo();
                    }

                    float fpsAlpha = config->FpsOverlayAlpha.value_or_default();
                    if (ImGui::SliderFloat("背景透明度", &fpsAlpha, 0.0f, 1.0f, "%.2f"))
                        config->FpsOverlayAlpha = fpsAlpha;

                    const char* options[] = { "Same as menu", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1", "1.2",
                                              "1.3",          "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
                    int currentIndex = std::max(((int) (config->FpsScale.value_or(0.0f) * 10.0f)) - 4, 0);
                    float values[] = { 0.0f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f,
                                       1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f };

                    if (ImGui::SliderInt("缩放", &currentIndex, 0, IM_ARRAYSIZE(options) - 1, options[currentIndex],
                                         ImGuiSliderFlags_ClampOnInput))
                    {
                        if (currentIndex == 0)
                            config->FpsScale.reset();
                        else
                            config->FpsScale = values[currentIndex];
                    }
                }

                // UPSCALER INPUTS -----------------------------
                ImGui::Spacing();
                auto uiStateOpen = currentFeature == nullptr || currentFeature->IsFrozen();
                if (auto ch =
                        ScopedCollapsingHeader("超分器输入", uiStateOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    if (config->EnableFsr2Inputs.value_or_default())
                    {
                        bool fsr2Inputs = config->UseFsr2Inputs.value_or_default();
                        bool fsr2Pattern = config->Fsr2Pattern.value_or_default();

                        if (ImGui::Checkbox("使用 FSR2 输入", &fsr2Inputs))
                            config->UseFsr2Inputs = fsr2Inputs;

                        if (ImGui::Checkbox("使用 FSR2 模式匹配", &fsr2Pattern))
                            config->Fsr2Pattern = fsr2Pattern;
                        ShowTooltip("此设置将在下次启动时生效！");
                    }

                    if (config->EnableFsr3Inputs.value_or_default())
                    {
                        bool fsr3Inputs = config->UseFsr3Inputs.value_or_default();
                        bool fsr3Pattern = config->Fsr3Pattern.value_or_default();

                        if (ImGui::Checkbox("使用 FSR3 输入", &fsr3Inputs))
                            config->UseFsr3Inputs = fsr3Inputs;

                        if (ImGui::Checkbox("使用 FSR3 模式匹配", &fsr3Pattern))
                            config->Fsr3Pattern = fsr3Pattern;
                        ShowTooltip("此设置将在下次启动时生效！");
                    }

                    if (config->EnableFfxInputs.value_or_default())
                    {
                        bool ffxInputs = config->UseFfxInputs.value_or_default();

                        if (ImGui::Checkbox("使用 FFX 输入", &ffxInputs))
                            config->UseFfxInputs = ffxInputs;
                    }
                }

                // DX11 & DX12 -----------------------------
                if (state.swapchainApi != Vulkan)
                {
                    // V-SYNC -----------------------------
                    ImGui::Spacing();
                    if (auto ch = ScopedCollapsingHeader("V-Sync 设置"); ch.IsHeaderOpen())
                    {
                        ScopedIndent indent {};
                        ImGui::Spacing();

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

                        ShowHelpMarker("控制 DXGI Present 同步间隔，它决定了\n"
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

                        ShowHelpMarker("强制 V-Sync 开关及同步间隔选项");

                        if (vsyncChanged && state.activeFgOutput == FGOutput::XeFG && state.currentFG != nullptr)
                        {
                            // To prevent XeLL issues
                            LOG_DEBUG("V-Sync change detected, forcing XeFG reset");
                            state.WAR_xefgRequestFGToggle = true;
                        }
                    }

                    // MIPMAP BIAS & Anisotropy -----------------------------
                    ImGui::Spacing();
                    if (auto ch = ScopedCollapsingHeader("Mipmap 偏移",
                                                         (currentFeature == nullptr || currentFeature->IsFrozen())
                                                             ? ImGuiTreeNodeFlags_DefaultOpen
                                                             : 0);
                        ch.IsHeaderOpen())
                    {
                        ScopedIndent indent {};
                        ImGui::Spacing();
                        if (config->MipmapBiasOverride.has_value() && _mipBias == 0.0f)
                            _mipBias = config->MipmapBiasOverride.value();

                        ImGui::SliderFloat("Mipmap 偏移##2", &_mipBias, -15.0f, 15.0f, "%.6f");
                        ShowHelpMarker("可帮助解决问题游戏中纹理模糊的问题\n"
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

                                ShowHelpMarker("对所有纹理应用相同的覆盖值");
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

                                ShowHelpMarker("将覆盖值作为缩放倍率应用\n"
                                               "使用缩放模式时，请使用正值\n"
                                               "覆盖值以增加锐化！");
                            }
                            ImGui::EndDisabled();

                            bool mbAll = config->MipmapBiasOverrideAll.value_or_default();
                            if (ImGui::Checkbox("覆盖所有纹理", &mbAll))
                                config->MipmapBiasOverrideAll = mbAll;

                            ShowHelpMarker("覆盖所有纹理的 mipmap 值\n"
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
                                ImGui::Text("当前：%.3f / %.3f，目标：%.3f", state.lastMipBias,
                                            state.lastMipBiasMax, config->MipmapBiasOverride.value());
                            }
                            else if (config->MipmapBiasScaleOverride.value_or_default())
                            {
                                ImGui::Text("当前：%.3f / %.3f，目标：基准 × %.3f", state.lastMipBias,
                                            state.lastMipBiasMax, config->MipmapBiasOverride.value());
                            }
                            else
                            {
                                ImGui::Text("当前：%.3f / %.3f，目标：基准 + %.3f", state.lastMipBias,
                                            state.lastMipBiasMax, config->MipmapBiasOverride.value());
                            }
                        }
                        else
                        {
                            ImGui::Text("当前：%.3f / %.3f", state.lastMipBias, state.lastMipBiasMax);
                        }

                        ImGui::Text("将在分辨率/预设更改后生效！！！");
                    }

                    ImGui::Spacing();
                    if (auto ch = ScopedCollapsingHeader("各向异性过滤",
                                                         (currentFeature == nullptr || currentFeature->IsFrozen())
                                                             ? ImGuiTreeNodeFlags_DefaultOpen
                                                             : 0);
                        ch.IsHeaderOpen())
                    {
                        ScopedIndent indent {};
                        ImGui::Spacing();
                        ImGui::PushItemWidth(65.0f * menuResScale);

                        auto selectedAF = config->AnisotropyOverride.has_value()
                                              ? std::to_string(config->AnisotropyOverride.value())
                                              : "自动";
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

                        ShowHelpMarker("更新比较过滤器");

                        ImGui::SameLine(0.0f, 6.0f);

                        bool afMinMax = config->AnisotropyModifyMinMax.value_or_default();
                        if (ImGui::Checkbox("修改最小/最大", &afMinMax))
                            config->AnisotropyModifyMinMax = afMinMax;

                        ShowHelpMarker("更新最小/最大过滤器");

                        bool afSkipPoint = config->AnisotropySkipPointFilter.value_or_default();
                        if (ImGui::Checkbox("跳过点过滤", &afSkipPoint))
                            config->AnisotropySkipPointFilter = afSkipPoint;

                        ShowHelpMarker("跳过点过滤器的更新");

                        ImGui::Text("可能在分辨率/预设更改后生效！！！");
                    }
                }

                ImGui::Spacing();
                if (auto ch = ScopedCollapsingHeader("按键绑定"); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

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

                ImGui::EndTable();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("plots", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("帧时间");
                    auto ft = StrFmt("%7.2f ms / %6.1f fps", frameTime, frameRate);
                    ImGui::PlotLines(
                        ft.c_str(), [](void* rb, int idx) -> float
                        { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, &gFrameTimes, plotWidth);

                    if (currentFeature != nullptr && !currentFeature->IsFrozen())
                    {
                        ImGui::TableNextColumn();
                        ImGui::Text("超分器");
                        auto ups = StrFmt("%7.2f ms", state.upscaleTimes.back());
                        ImGui::PlotLines(
                            ups.c_str(), [](void* rb, int idx) -> float
                            { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, &gUpscalerTimes,
                            plotWidth);
                    }

                    ImGui::EndTable();
                }

                // BOTTOM LINE ---------------
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (currentFeature != nullptr && !currentFeature->IsFrozen())
                {
                    ImGui::Text("%dx%d -> %dx%d (%.1f) [%dx%d (%.1f)]", currentFeature->RenderWidth(),
                                currentFeature->RenderHeight(), currentFeature->TargetWidth(),
                                currentFeature->TargetHeight(),
                                (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth(),
                                currentFeature->DisplayWidth(), currentFeature->DisplayHeight(),
                                (float) currentFeature->DisplayWidth() / (float) currentFeature->RenderWidth());

                    ImGui::SameLine(0.0f, 4.0f);

                    ImGui::Text("%d", currentFeature->FrameCount());

                    ImGui::SameLine(0.0f, 10.0f);
                }

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

                ImGui::SameLine(0.0f, 15.0f);

                if (ImGui::Button("保存设置"))
                    config->SaveIni();

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button("关闭"))
                {
                    _isVisible = false;
                    hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
                    io.BackendFlags &= 30;
                    io.ConfigFlags =
                        ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

                    if (pfn_ClipCursor_hooked)
                        pfn_ClipCursor(&_cursorLimit);

                    _showMipmapCalcWindow = false;
                    _showHudlessWindow = false;
                    io.MouseDrawCursor = false;
                    io.WantCaptureKeyboard = false;
                    io.WantCaptureMouse = false;
                }

                auto winSize = ImGui::GetWindowSize();
                auto winPos = ImGui::GetWindowPos();

                ImGui::SameLine();

                auto textSize = ImGui::CalcTextSize("打开 Wiki (?)");
                auto& style = ImGui::GetStyle();
                textSize.x += style.FramePadding.x * 2.0f;
                textSize.x += style.ItemSpacing.x;

                float avail = ImGui::GetContentRegionAvail().x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textSize.x);

                // Make button text underline
                if (ImGui::Button("打开 Wiki"))
                {
                    auto pIO = &ImGui::GetPlatformIO();
                    auto ctx = ImGui::GetCurrentContext();
                    pIO->Platform_OpenInShellFn(ctx, "https://github.com/optiscaler/OptiScaler/wiki");
                }
                ShowHelpMarker("点击在默认浏览器中打开 OptiScaler Wiki 页面\n\n"
                               "已知游戏问题与解决方法的兼容性列表\n以及帧生成选项说明\n"
                               "和其他有用信息");

                ImGui::Spacing();
                ImGui::Separator();

                if (state.nvngxIniDetected)
                {
                    ImGui::Spacing();
                    ImGui::TextColored(
                        toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)),
                        "检测到 nvngx.ini，请改用 OptiScaler.ini 并删除旧配置");
                    ImGui::Spacing();
                }

                if (lastPosition.x < -900.0f ||
                    (lastPosition.x >= winPos.x - 1.0f && lastPosition.y >= winPos.y - 1.0f &&
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

                ImGui::End();
            }

            // Metrics window (for debug)
            // ImGui::ShowMetricsWindow();

            // Mipmap calculation window
            if (_showMipmapCalcWindow && currentFeature != nullptr && !currentFeature->IsFrozen() &&
                currentFeature->IsInited())
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
                                _displayWidth =
                                    static_cast<uint32_t>(currentFeature->DisplayWidth() *
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

                    const char* q[] = { "极致性能", "性能",   "均衡",
                                        "质量",           "极致质量", "DLAA" };
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

                    if (ImGui::BeginTable("HudlessTable", 2, ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthFixed);

                        ankerl::unordered_dense::map<void*, CapturedHudlessInfo>::iterator it;

                        for (it = state.CapturedHudlesses.begin(); it != state.CapturedHudlesses.end(); it++)
                        {
                            ImGui::TableNextRow();

                            ImGui::TableSetColumnIndex(0);

                            ImGui::Text("%08x, %s->%s, Count: %llu, %s", (size_t) it->first,
                                        GetSourceString(it->second.captureInfo & 0xFF).c_str(),
                                        GetDispatchString(it->second.captureInfo & 0xFF00).c_str(),
                                        it->second.usageCount, it->second.enabled ? "活跃" : "被动");

                            ImGui::TableSetColumnIndex(1);

                            btnCount++;
                            std::string text;

                            if (it->second.enabled)
                                text = StrFmt("禁用##%d", btnCount);
                            else
                                text = StrFmt("启用##%d", btnCount);

                            if (ImGui::Button(text.c_str()))
                            {
                                LOG_DEBUG("Hudless {:X}: {}", (size_t) it->first,
                                          it->second.enabled ? "Disabling" : "Enabling");
                                it->second.enabled = !it->second.enabled;
                            }
                        }

                        ImGui::EndTable();
                    }

                    if (ImGui::Button("清除##4"))
                    {
                        LOG_DEBUG("Clearing captured hudless resources");
                        state.ClearCapturedHudlesses = true;
                    }

                    ImGui::SameLine(0.0f, 8.0f);

                    if (ImGui::Button("关闭##4"))
                        _showHudlessWindow = false;

                    ImGui::End();
                }
            }
        }

        if (config->UseHQFont.value_or_default())
            ImGui::PopFontSize();
    }

    if (newFrame)
        ImGui::EndFrame();

    return newFrame;
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

    // [CN] Merge the embedded CJK font so Chinese UI text renders correctly
    {
        ImFontAtlas* atlas = io.Fonts;

        // Fallback: ensure at least one font exists (e.g. UseHQFont disabled -> default ImGui font)
        if (atlas->Fonts.empty())
            io.FontDefault = atlas->AddFontDefault();

        ImFontConfig cjkConfig;
        cjkConfig.MergeMode = true;
        cjkConfig.FontNo = 0;
        cjkConfig.OversampleH = 1;
        cjkConfig.OversampleV = 1;

        static const ImWchar cjkGlyphRanges[] = { 0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
                                                  0x2000, 0x2BFF, // Punctuation, symbols, arrows, math
                                                  0x3000, 0x303F, // CJK Symbols and Punctuation
                                                  0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
                                                  0x4E00, 0x9FAF, // CJK Unified Ideographs
                                                  0 };
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

    if (hwndPid == GetCurrentProcessId() && !Config::Instance()->ManualInputPolling.value_or_default())
    {
        inputManual = false;

        if ((_oWndProc == nullptr || oldHandle != _handle) && !isUWP)
        {
            if (oldHandle != nullptr && _oWndProc != nullptr)
            {
                LOG_DEBUG("Restoring old WndProc: {:X}", (ULONG64) _oWndProc);

                SetLastError(0);
                auto restoreResult = SetWindowLongPtr(oldHandle, GWLP_WNDPROC, (LONG_PTR) _oWndProc);
                auto error = GetLastError();

                if (restoreResult == 0 && error != 0)
                {
                    LOG_ERROR("Failed to restore old WndProc. Error: {:X}", error);
                }
            }

            SetLastError(0);
            auto setResult = (WNDPROC) SetWindowLongPtr(_handle, GWLP_WNDPROC, (LONG_PTR) WndProc);
            auto error = GetLastError();

            if (setResult == nullptr && error != 0)
            {
                LOG_ERROR("Failed to hook WndProc. Error: {:X}", error);
            }
            else
            {
                _oWndProc = setResult;
                LOG_DEBUG("_oWndProc: {:X}", (ULONG64) _oWndProc);
            }
        }
    }
    else
    {
        if (!Config::Instance()->ManualInputPolling.value_or_default())
            LOG_WARN("HWND does not belong to current process, Manual input polling will be used");
        else
            LOG_DEBUG("Manual input polling enabled by config");

        inputManual = true;
    }

    ApplyThemeStyle();
    _isInited = true;
}

void MenuCommon::Shutdown()
{
    if (!MenuCommon::_isInited)
        return;

    if (_oWndProc != nullptr)
    {
        auto handle = (HWND) ImGui::GetMainViewport()->PlatformHandleRaw;
        SetLastError(0);
        auto restoreResult = SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR) _oWndProc);
        auto error = GetLastError();

        if (restoreResult == 0 && error != 0)
        {
            LOG_ERROR("Failed to restore old WndProc. Error: {:X}", error);
        }

        _oWndProc = nullptr;
    }

    if (pfn_SetCursorPos_hooked)
        DetachHooks();

    if (!_isUWP)
        ImGui_ImplWin32_Shutdown();
    else
        ImGui_ImplUwp_Shutdown();

    ImGui::DestroyContext();

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

    if (pfn_ClipCursor_hooked)
        pfn_ClipCursor(&_cursorLimit);

    _showMipmapCalcWindow = false;
    _showHudlessWindow = false;

    RECT windowRect = {};

    if (!_isUWP && GetWindowRect(_handle, &windowRect))
    {
        auto x = windowRect.left + (windowRect.right - windowRect.left) / 2;
        auto y = windowRect.top + (windowRect.bottom - windowRect.top) / 2;

        if (pfn_SetCursorPos != nullptr)
            pfn_SetCursorPos(x, y);
        else
            SetCursorPos(x, y);
    }

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
}
