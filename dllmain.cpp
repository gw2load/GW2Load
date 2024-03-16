// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include <d3d11.h>
#include <Shlobj.h>
#include <filesystem>
#include <set>
#include <iostream>
#include <map>
#include <string_view>

HMODULE g_Module;
std::set<HWND> g_HookedHWNDs;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (msg == WM_PAINT)
        InitializeHook(hWnd);

    if (msg == WM_PAINT || msg == WM_CREATE)
    {
        auto dbg = std::format("hwnd={} msg={}\n", (void*)hWnd, GetWndProcMessageName(msg));
        OutputDebugStringA(dbg.c_str());
    }

    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

HHOOK g_callWndProcHook = nullptr;
LRESULT CALLBACK CallWndProcHook(int nCode, WPARAM wParam, LPARAM lParam) {
    auto hwnd = ((CWPSTRUCT*)lParam)->hwnd;
    if (nCode == HC_ACTION && hwnd && g_HookedHWNDs.count(hwnd) == 0) {
        SetWindowSubclass(hwnd, WndProc, 0, 0);
        g_HookedHWNDs.insert(hwnd);
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

void Init();
HMODULE g_RealDLL = nullptr;

#define FUNC_EXPORT(Name_, Return_, Arguments_, Parameters_) \
    using Name_##_t = Return_(WINAPI*)Arguments_; \
    Name_##_t g_##Name_ = nullptr; \
    extern "C" Return_ WINAPI H##Name_ Arguments_ { \
        if(!g_RealDLL) Init(); \
        return g_##Name_ Parameters_; \
    }

FUNC_EXPORT(vSetDdrawflag, void, (), ());
FUNC_EXPORT(AlphaBlend, BOOL, (HDC hdcDest, int xoriginDest, int yoriginDest, int wDest, int hDest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wSrc, int hSrc, BLENDFUNCTION ftn), (hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, ftn));
FUNC_EXPORT(DllInitialize, void, (), ());
FUNC_EXPORT(GradientFill, BOOL, (HDC hdc, PTRIVERTEX pVertex, ULONG nVertex, PVOID pMesh, ULONG nMesh, ULONG ulMode), (hdc, pVertex, nVertex, pMesh, nMesh, ulMode));
FUNC_EXPORT(TransparentBlt, BOOL, (HDC hdcDest, int xoriginDest, int yoriginDest, int wDest, int hDest, HDC hdcSrc, int xoriginSrc, int yoriginSrc, int wSrc, int hSrc, UINT crTransparent), (hdcDest, xoriginDest, yoriginDest, wDest, hDest, hdcSrc, xoriginSrc, yoriginSrc, wSrc, hSrc, crTransparent));


#define FUNC_LOAD(Name_) \
    g_##Name_ = reinterpret_cast<Name_##_t>(GetProcAddress(g_RealDLL, #Name_));

void Init()
{
    PWSTR path;
    SHGetKnownFolderPath(FOLDERID_System, 0, nullptr, &path);
    std::filesystem::path p(path);
    p /= "msimg32.dll";
    g_RealDLL = LoadLibraryW(p.wstring().c_str());
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
        g_Module = hModule;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        if (g_RealDLL)
        {
            FreeLibrary(g_RealDLL);
            g_RealDLL = nullptr;
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