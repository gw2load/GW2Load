// dllmain.cpp : Defines the entry point for the DLL application.
#include "Loader.h"
#include "Utils.h"
#include <d3d11.h>
#include <Shlobj.h>

#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

HWND g_LauncherWindow = nullptr;
HMODULE g_MSIMG32Handle = nullptr;
HMODULE g_LoaderModuleHandle = nullptr;
bool g_FirstWindowCreated = false;
bool g_MainWindowCreated = false;

HHOOK g_callWndProcHook = nullptr;
LRESULT CALLBACK CallWndProcHook(int nCode, WPARAM wParam, LPARAM lParam) {
    const auto* message = reinterpret_cast<const CWPSTRUCT*>(lParam);
    if (nCode == HC_ACTION && message->hwnd)
    {
        if (!g_LauncherWindow)
            g_LauncherWindow = message->hwnd;

        //spdlog::debug("msg = {} hwnd = {:x}", GetWndProcMessageName(message->message), reinterpret_cast<std::uintptr_t>(message->hwnd));
        if (message->message == WM_CREATE)
        {
            if (!g_FirstWindowCreated)
            {
                Initialize(InitializationType::BeforeFirstWindow, message->hwnd);
                g_FirstWindowCreated = true;
            }
            
            using namespace std::literals::string_view_literals;

            const auto* create = reinterpret_cast<const CREATESTRUCT*>(message->lParam);
            if (HIWORD(create->lpszClass) != 0 && std::wstring_view{ create->lpszClass } == L"ArenaNet_Gr_Window_Class"sv)
            {
                Initialize(InitializationType::BeforeGameWindow, message->hwnd);
                g_MainWindowCreated = true;
            }
        }
        else if (message->message == WM_DESTROY)
        {
            if (g_MainWindowCreated)
                Quit(message->hwnd);
            else if (g_LauncherWindow == message->hwnd)
            {
                LauncherClosing(g_LauncherWindow);
                g_LauncherWindow = nullptr;
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

bool IsAttachedToGame()
{
    return g_callWndProcHook != nullptr;
}

void Init();

#define FUNC_EXPORT(Name_, Return_, Arguments_, Parameters_) \
    using Name_##_t = Return_(WINAPI*)Arguments_; \
    Name_##_t g_##Name_ = nullptr; \
    extern "C" Return_ WINAPI H##Name_ Arguments_ { \
        if(!g_MSIMG32Handle) Init(); \
        return g_##Name_ Parameters_; \
    }

FUNC_EXPORT(vSetDdrawflag, void, (), ());
FUNC_EXPORT(AlphaBlend, BOOL, (HDC hdcDest, int xoriginDest, int yoriginDest, int wDest, int hDest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wSrc, int hSrc, BLENDFUNCTION ftn), (hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, ftn));
FUNC_EXPORT(DllInitialize, void, (), ());
FUNC_EXPORT(GradientFill, BOOL, (HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode), (hdc, pVertex, nVertex, pMesh, nMesh, ulMode));
FUNC_EXPORT(TransparentBlt, BOOL, (HDC hdcDest, int xoriginDest, int yoriginDest, int wDest, int hDest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wSrc, int hSrc, UINT crTransparent), (hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, crTransparent));


#define FUNC_LOAD(Name_) \
    g_##Name_ = reinterpret_cast<Name_##_t>(GetProcAddress(g_MSIMG32Handle, #Name_))

std::shared_ptr<spdlog::logger> g_Logger;

bool ValidateExecutable()
{
    wchar_t exeName[MAX_PATH];
    GetModuleFileNameW(nullptr, exeName, sizeof(exeName));
    DWORD verHandle;
    DWORD verSize = GetFileVersionInfoSizeW(exeName, &verHandle);
    if (verSize == 0)
        return false;

    std::vector<unsigned char> verData(verSize);
    if (!GetFileVersionInfoW(exeName, verHandle, verSize, verData.data()))
        return false;

    const LANGANDCODEPAGE* lpTranslate = nullptr;
    const LANGANDCODEPAGE fallbackTranslate{ 0x0409, 0x04B0 };

    UINT cbTranslate;
    if (!VerQueryValueW(verData.data(), L"\\VarFileInfo\\Translation", (void**)&lpTranslate, &cbTranslate))
    {
        lpTranslate = &fallbackTranslate;
    }

    unsigned int fileInfoSize;
    wchar_t* fileInfoBuf;
    const auto query = std::format(L"\\StringFileInfo\\{:04x}{:04x}\\ProductName", lpTranslate->wLanguage, lpTranslate->wCodePage);
    if (!VerQueryValueW(verData.data(), query.c_str(), (void**)&fileInfoBuf, &fileInfoSize) || fileInfoSize == 0)
        return false;

    if (std::wstring_view{ fileInfoBuf } != L"Guild Wars 2")
        return false;

    return true;
}

void Init()
{
    PWSTR path;
    SHGetKnownFolderPath(FOLDERID_System, 0, nullptr, &path);
    std::filesystem::path p(path);
    p /= "msimg32.dll";
    g_MSIMG32Handle = LoadLibraryW(p.wstring().c_str());
    FUNC_LOAD(vSetDdrawflag);
    FUNC_LOAD(AlphaBlend);
    FUNC_LOAD(DllInitialize);
    FUNC_LOAD(GradientFill);
    FUNC_LOAD(TransparentBlt);
    CoTaskMemFree(path);

    if (!ValidateExecutable())
        return;

    std::filesystem::create_directories("addons/_logs/GW2Load");
    auto outputLogger = std::make_shared<spdlog::sinks::msvc_sink_mt>(true);
    outputLogger->set_pattern("[GW2Load>%l|%T.%f] %v");

#ifdef _DEBUG
    auto max_log_size = 1_mb;
#else
    auto max_log_size = 100_kb;
#endif

    auto fileLogger = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("addons/_logs/GW2Load/GW2Load.log", max_log_size, 10, true);
    fileLogger->set_pattern("%Y-%m-%d %T.%f [%l] %v");
    g_Logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{ outputLogger, fileLogger });
    spdlog::set_default_logger(g_Logger);

#ifdef _DEBUG
    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::set_level(spdlog::level::trace);
#else
    spdlog::flush_every(std::chrono::seconds(5));
    spdlog::set_level(spdlog::level::info);
#endif

    if(!g_callWndProcHook)
        g_callWndProcHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProcHook, nullptr, GetCurrentThreadId());

    spdlog::info("Initializing GW2Load...");
    Initialize(InitializationType::InLauncher, std::nullopt);
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_LoaderModuleHandle = hModule;
        break;
    case DLL_PROCESS_DETACH:
        if (g_MSIMG32Handle)
        {
            FreeLibrary(g_MSIMG32Handle);
            g_MSIMG32Handle = nullptr;
        }
        if (g_callWndProcHook)
        {
            UnhookWindowsHookEx(g_callWndProcHook);
            g_callWndProcHook = nullptr;
        }
        break;
    default:
        break;
    }
    return TRUE;
}