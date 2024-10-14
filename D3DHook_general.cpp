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

DECLARE_HOOK(D3D11CreateDevice)(
	_In_opt_ IDXGIAdapter* pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	_In_reads_opt_(FeatureLevels) CONST D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	_COM_Outptr_opt_ ID3D11Device** ppDevice,
	_Out_opt_ D3D_FEATURE_LEVEL* pFeatureLevel,
	_COM_Outptr_opt_ ID3D11DeviceContext** ppImmediateContext)
{
	return RealD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
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

	HOOK_OR_RETURN(g_D3D11Handle, D3D11CreateDevice);

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
			InitializeAddons(false);
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

IDXGISwapChain1* GetSwapChain1(IDXGISwapChain* sc)
{
	IDXGISwapChain1* sc1 = nullptr;
	if (SUCCEEDED(sc->QueryInterface(&sc1)))
		return sc1;
	else
		return nullptr;
}

IDXGISwapChain3* GetSwapChain3(IDXGISwapChain* sc)
{
	IDXGISwapChain3* sc3 = nullptr;
	if (SUCCEEDED(sc->QueryInterface(&sc3)))
		return sc3;
	else
		return nullptr;
}

IDXGISwapChain* Downcast(IDXGISwapChain1* swc)
{
	return swc;
}

IDXGISwapChain* Downcast(IDXGISwapChain3* swc)
{
	return swc;
}
