// dllmain.cpp : Defines the entry point for the DLL application.
#include "Loader.h"
#include "Utils.h"
#include <d3d11.h>
#include <Shlobj.h>

HMODULE g_MSIMG32Handle = nullptr;
HMODULE g_LoaderModuleHandle = nullptr;
bool g_FirstWindowCreated = false;
bool g_MainWindowCreated = false;

HHOOK g_callWndProcHook = nullptr;
LRESULT CALLBACK CallWndProcHook(int nCode, WPARAM wParam, LPARAM lParam) {
    const auto* message = reinterpret_cast<const CWPSTRUCT*>(lParam);
    if (nCode == HC_ACTION && message->hwnd)
    {
        spdlog::debug("msg = {} hwnd = {:x}\n", GetWndProcMessageName(message->message), reinterpret_cast<std::uintptr_t>(message->hwnd));
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
            Quit(message->hwnd);
        }
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
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
    g_##Name_ = reinterpret_cast<Name_##_t>(GetProcAddress(g_MSIMG32Handle, #Name_));

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

    if(!g_callWndProcHook)
        g_callWndProcHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProcHook, nullptr, GetCurrentThreadId());
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_LoaderModuleHandle = hModule;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
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
    }
    return TRUE;
}