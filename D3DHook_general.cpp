#include "D3DHook.h"
#include "Utils.h"
#include "Loader.h"

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

bool InitializeD3DHook()
{
	static bool initialized = false;

	spdlog::debug("Attempting to initialize D3D hook.");

	if (initialized)
	{
		spdlog::debug("D3D hook already initialized!");
		return false;
	}

	ComPtr<IDXGIFactory> factory;
	if(auto hr = CreateDXGIFactory(IID_PPV_ARGS(&factory)); FAILED(hr))
	{
		spdlog::error("Could not create DXGIFactory!");
		return false;
	}

	OverwriteFactoryVTables(factory.Get());

	initialized = true;
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

		spdlog::debug("Updating swapchain from {} to {}...", fmt::ptr(g_SwapChain), fmt::ptr(swc));
		if (g_SwapChain) g_SwapChain->Release();
		g_SwapChain = swc;
		g_SwapChain->AddRef();

		DXGI_SWAP_CHAIN_DESC desc;
		g_SwapChain->GetDesc(&desc);
		g_AssociatedWindow = desc.OutputWindow;

		if (SUCCEEDED(g_SwapChain->GetDevice(IID_PPV_ARGS(&g_Device))))
		{
			spdlog::debug("Updating device to {}...", fmt::ptr(g_Device));
			g_Device->GetImmediateContext(&g_DeviceContext);
			spdlog::debug("Updating immediate context to {}...", fmt::ptr(g_DeviceContext));
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

IDXGISwapChain* Downcast(IDXGISwapChain* swc) { return swc; }
IDXGISwapChain* Downcast(IDXGISwapChain1* swc) { return swc; }
IDXGISwapChain* Downcast(IDXGISwapChain2* swc) { return swc; }
IDXGISwapChain* Downcast(IDXGISwapChain3* swc) { return swc; }
IDXGISwapChain* Downcast(IDXGISwapChain4* swc) { return swc; }

IDXGIFactory* Downcast(IDXGIFactory* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory1* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory2* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory3* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory4* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory5* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory6* f) { return f; }
IDXGIFactory* Downcast(IDXGIFactory7* f) { return f; }

#define MAKE_UUID_GETTER(Type) \
	template<> REFIID GetUUIDOf<Type>() { return __uuidof(Type); }

MAKE_UUID_GETTER(IDXGISwapChain);
MAKE_UUID_GETTER(IDXGISwapChain1);
MAKE_UUID_GETTER(IDXGISwapChain2);
MAKE_UUID_GETTER(IDXGISwapChain3);
MAKE_UUID_GETTER(IDXGISwapChain4);

MAKE_UUID_GETTER(IDXGIFactory);
MAKE_UUID_GETTER(IDXGIFactory1);
MAKE_UUID_GETTER(IDXGIFactory2);
MAKE_UUID_GETTER(IDXGIFactory3);
MAKE_UUID_GETTER(IDXGIFactory4);
MAKE_UUID_GETTER(IDXGIFactory5);
MAKE_UUID_GETTER(IDXGIFactory6);
MAKE_UUID_GETTER(IDXGIFactory7);

#undef MAKE_UUID_GETTER