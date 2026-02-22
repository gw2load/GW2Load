#include "D3DHook.h"
#include "Utils.h"
#include "Loader.h"

#include <MinHook.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

decltype(CreateDXGIFactory)* RealCreateDXGIFactory = nullptr;
HRESULT WINAPI HkCreateDXGIFactory(REFIID riid, void** ppFactory) {
    HRESULT hr = RealCreateDXGIFactory(riid, ppFactory);
    if(SUCCEEDED(hr))
        OverwriteFactoryVTables(*ppFactory);
    return hr;
}

decltype(CreateDXGIFactory1)* RealCreateDXGIFactory1 = nullptr;
HRESULT WINAPI HkCreateDXGIFactory1(REFIID riid, void** ppFactory) {
    HRESULT hr = RealCreateDXGIFactory1(riid, ppFactory);
    if(SUCCEEDED(hr))
        OverwriteFactoryVTables(*ppFactory);
    return hr;
}

decltype(CreateDXGIFactory2)* RealCreateDXGIFactory2 = nullptr;
HRESULT WINAPI HkCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    HRESULT hr = RealCreateDXGIFactory2(Flags, riid, ppFactory);
    if(SUCCEEDED(hr))
        OverwriteFactoryVTables(*ppFactory);
    return hr;
}

void HookDXGIFactories() {
    MH_Initialize();
    MH_CreateHookApi(L"dxgi.dll", "CreateDXGIFactory", HkCreateDXGIFactory, reinterpret_cast<LPVOID*>(&RealCreateDXGIFactory));
    MH_CreateHookApi(L"dxgi.dll", "CreateDXGIFactory1", HkCreateDXGIFactory1, reinterpret_cast<LPVOID*>(&RealCreateDXGIFactory1));
    MH_CreateHookApi(L"dxgi.dll", "CreateDXGIFactory2", HkCreateDXGIFactory2, reinterpret_cast<LPVOID*>(&RealCreateDXGIFactory2));
    MH_EnableHook(MH_ALL_HOOKS);
}