module;
#include "framework.h"
#include <d3d11.h>
#include <wrl/client.h>

export module D3DHook;

import std;
import VTables;

using namespace Microsoft::WRL;

std::unordered_set<HWND> g_D3DKnownHWNDs;

export bool InitializeD3DHook(HWND hWnd)
{
	if (g_D3DKnownHWNDs.contains(hWnd))
		return false;

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

	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &tempSwapChain, &tempDevice, NULL, &tempContext)))
		return false;

	OverwriteVTables(tempSwapChain.Get(), tempDevice.Get(), tempContext.Get());

	return true;
}