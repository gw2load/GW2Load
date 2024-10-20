#include "D3DHook.h"
#include "Utils.h"
#include "Loader.h"

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

extern HMODULE g_D3D11Handle;
extern HMODULE g_DXGIHandle;

#define DECLARE_HOOK(Name_) \
decltype(Name_)* Real##Name_ = nullptr; \
HRESULT WINAPI Hk##Name_

DECLARE_HOOK(D3D11CreateDeviceAndSwapChain)(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_In_opt_ CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
	_COM_Outptr_opt_ IDXGISwapChain** ppSwapChain,
	_COM_Outptr_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext)
{
	spdlog::debug("SwapChain initialization performed via D3D11CreateDeviceAndSwapChain.");

	auto rval = RealD3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
	if (SUCCEEDED(rval) && ppSwapChain)
		InitializeSwapChain(*ppSwapChain);
	return rval;
}

DECLARE_HOOK(CreateDXGIFactory)(REFIID riid, void** ppFactory)
{
	auto rval = RealCreateDXGIFactory(riid, ppFactory);
	if (SUCCEEDED(rval) && ppFactory)
		OverwriteDXGIFactoryVTables(GetFactoryPointer(riid, *ppFactory));
	return rval;
}

DECLARE_HOOK(CreateDXGIFactory1)(REFIID riid, void** ppFactory)
{
	auto rval = RealCreateDXGIFactory1(riid, ppFactory);
	if (SUCCEEDED(rval) && ppFactory)
		OverwriteDXGIFactoryVTables(GetFactoryPointer(riid, *ppFactory));
	return rval;
}

DECLARE_HOOK(CreateDXGIFactory2)(UINT Flags, REFIID riid, void** ppFactory)
{
	auto rval = RealCreateDXGIFactory2(Flags, riid, ppFactory);
	if (SUCCEEDED(rval) && ppFactory)
		OverwriteDXGIFactoryVTables(GetFactoryPointer(riid, *ppFactory));
	return rval;
}

#undef DECLARE_HOOK

bool InitializeD3DHook()
{
	if (!g_D3D11Handle || !g_DXGIHandle)
	{
		spdlog::critical("Cannot initialize D3D hooks, invalid handle(s): D3D11Handle = {}; DXGIHandle = {}", fmt::ptr(g_D3D11Handle), fmt::ptr(g_DXGIHandle));
		return false;
	}

	if (auto err = MH_Initialize(); err != MH_OK)
	{
		spdlog::critical("Cannot initialize D3D hooks: cannot initialize MinHook, error code {}", err);
		return false;
	}

#define HOOK_OR_RETURN(Mod_, Name_) \
	{ auto r = DetourLibraryFunction(Mod_, #Name_, Hk##Name_); if(r.has_value()) Real##Name_ = *r; else { spdlog::critical("Could not hook {}: error code {}!", #Name_, r.error()); return false; } }

	HOOK_OR_RETURN(g_D3D11Handle, D3D11CreateDeviceAndSwapChain);
	HOOK_OR_RETURN(g_DXGIHandle, CreateDXGIFactory);
	HOOK_OR_RETURN(g_DXGIHandle, CreateDXGIFactory1);
	HOOK_OR_RETURN(g_DXGIHandle, CreateDXGIFactory2);

	if (auto err = MH_EnableHook(MH_ALL_HOOKS); err != MH_OK)
	{
		spdlog::critical("Cannot initialize D3D hooks: cannot enable MinHook, error code {}", err);
		return false;
	}

#undef HOOK_OR_RETURN

	return true;
}

IDXGISwapChain* g_SwapChain = nullptr;
ID3D11Device* g_Device = nullptr;
ID3D11DeviceContext* g_DeviceContext = nullptr;
HWND g_AssociatedWindow = nullptr;

void InitializeD3DObjects(IDXGISwapChain* swc)
{
	if (g_SwapChain != nullptr && g_SwapChain != swc)
		spdlog::warn("Swapchain changed after initialization!");

	if (g_SwapChain != swc)
	{
		const bool firstInit = g_SwapChain == nullptr;

		spdlog::debug("Updating swapchain from {} to {}...", fptr(g_SwapChain), fptr(swc));
		if (g_SwapChain) g_SwapChain->Release();
		g_SwapChain = swc;
		g_SwapChain->AddRef();

		DXGI_SWAP_CHAIN_DESC desc;
		g_SwapChain->GetDesc(&desc);
		g_AssociatedWindow = desc.OutputWindow;

		if (SUCCEEDED(g_SwapChain->GetDevice(IID_PPV_ARGS(&g_Device))))
		{
			spdlog::debug("Updating device to {}...", fptr(g_Device));
			g_Device->GetImmediateContext(&g_DeviceContext);
			spdlog::debug("Updating immediate context to {}...", fptr(g_DeviceContext));
		}
		else
			spdlog::error("Could not get device from swapchain!");

		if (firstInit)
			Initialize(InitializationType::AfterSwapChainCreated, g_AssociatedWindow);
	}
}

void ShutdownD3DObjects(HWND hWnd)
{
	if (hWnd != g_AssociatedWindow)
		return;

	if (g_DeviceContext)
	{
		spdlog::debug("Destroying immediate context...");
		g_DeviceContext->Release();
		g_DeviceContext = nullptr;
	}
	if (g_Device)
	{
		spdlog::debug("Destroying device...");
		g_Device->Release();
		g_Device = nullptr;
	}
	if (g_SwapChain)
	{
		spdlog::debug("Destroying swapchain...");
		g_SwapChain->Release();
		g_SwapChain = nullptr;
	}

	g_AssociatedWindow = nullptr;

	RestoreVTables();
}

std::pair<ID3D11Device*, ID3D11DeviceContext*> GetDeviceFromSwapChain(IDXGISwapChain* sc)
{
	ID3D11Device* dev;
	ID3D11DeviceContext* ctx;
	sc->GetDevice(IID_PPV_ARGS(&dev));
	if (!dev)
		return { nullptr, nullptr };

	dev->GetImmediateContext(&ctx);
	return { dev, ctx };
}

IDXGIFactory* GetFactoryPointer(REFIID riid, void* factory)
{
	if (riid == __uuidof(IDXGIFactory))
		return reinterpret_cast<IDXGIFactory*>(factory);
	if (riid == __uuidof(IDXGIFactory1))
		return reinterpret_cast<IDXGIFactory1*>(factory);
	if (riid == __uuidof(IDXGIFactory2))
		return reinterpret_cast<IDXGIFactory2*>(factory);
	if (riid == __uuidof(IDXGIFactory3))
		return reinterpret_cast<IDXGIFactory3*>(factory);
	if (riid == __uuidof(IDXGIFactory4))
		return reinterpret_cast<IDXGIFactory4*>(factory);
	if (riid == __uuidof(IDXGIFactory5))
		return reinterpret_cast<IDXGIFactory5*>(factory);
	if (riid == __uuidof(IDXGIFactory6))
		return reinterpret_cast<IDXGIFactory6*>(factory);
	if (riid == __uuidof(IDXGIFactory7))
		return reinterpret_cast<IDXGIFactory7*>(factory);
	spdlog::critical("Could not recognize UUID {:X}-{:X}-{:X}-{:X} as IDXGIFactory child class!", riid.Data1, riid.Data2, riid.Data3, fmt::join(riid.Data4, ""));
	return nullptr;
}

#define QUERY_VERSIONED_INTERFACE(Name_, Prefix_, Version_) \
Prefix_##Name_##Version_* Get##Name_##Version_(Prefix_##Name_* obj) \
{ \
	Prefix_##Name_##Version_* vobj = nullptr; \
	if (SUCCEEDED(obj->QueryInterface(&vobj))) \
		return vobj; \
	else \
		return nullptr; \
} \
Prefix_##Name_* Downcast(Prefix_##Name_##Version_* obj) \
{ \
	return obj; \
}

QUERY_VERSIONED_INTERFACE(SwapChain, IDXGI, 1);
QUERY_VERSIONED_INTERFACE(SwapChain, IDXGI, 3);

QUERY_VERSIONED_INTERFACE(Factory, IDXGI, 1);
QUERY_VERSIONED_INTERFACE(Factory, IDXGI, 2);
