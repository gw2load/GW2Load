#include "D3DHook.h"
#include "Utils.h"
#include "Loader.h"

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

std::unordered_set<HWND> g_D3DKnownHWNDs;

bool InitializeD3DHook(HWND hWnd)
{
	spdlog::debug("Attempting to initialize D3D hook for window {}.", reinterpret_cast<void*>(hWnd));

	if (g_D3DKnownHWNDs.contains(hWnd))
	{
		spdlog::debug("Skipping window {} because it was already hooked.", reinterpret_cast<void*>(hWnd));
		return false;
	}

	g_D3DKnownHWNDs.insert(hWnd);

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = (GetWindowLong(hWnd, GWL_STYLE) & WS_POPUP) != 0 ? FALSE : TRUE;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	ComPtr<IDXGISwapChain> tempSwapChain;
	ComPtr<ID3D11Device> tempDevice;
	ComPtr<ID3D11DeviceContext> tempContext;

	if (auto hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &tempSwapChain, &tempDevice, NULL, &tempContext); FAILED(hr))
	{
		spdlog::error("Failed to hook window {} due to failed D3D11CreateDeviceAndSwapChain: error code {:x}.", reinterpret_cast<void*>(hWnd), hr);
		return false;
	}

	OverwriteVTables(tempSwapChain.Get(), tempDevice.Get(), tempContext.Get());

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
