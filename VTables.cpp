#include "framework.h"

#include <set>

#define CINTERFACE
#define D3D11_NO_HELPERS
#include <d3d11.h>

decltype(IDXGISwapChainVtbl::Present) RealPresent = nullptr;
HRESULT STDMETHODCALLTYPE HkPresent(
	IDXGISwapChain* This,
	UINT SyncInterval,
	UINT Flags)
{
	return RealPresent(This, SyncInterval, Flags);
}

std::set<IDXGISwapChainVtbl*> g_SwapChainTables;
std::set<ID3D11DeviceVtbl*> g_DeviceTables;
std::set<ID3D11DeviceContextVtbl*> g_DeviceContextTables;

void OverwriteVTables(void* sc, void* dev, void* ctx)
{
	auto* swapChainVT = reinterpret_cast<IDXGISwapChain*>(sc)->lpVtbl;
	auto* deviceVT = reinterpret_cast<ID3D11Device*>(dev)->lpVtbl;
	auto* contextVT = reinterpret_cast<ID3D11DeviceContext*>(ctx)->lpVtbl;

	if (!g_SwapChainTables.contains(swapChainVT))
	{
		DWORD oldProtect;
		VirtualProtect(&swapChainVT->Present, sizeof(void*), PAGE_READWRITE, &oldProtect);

		RealPresent = swapChainVT->Present;
		swapChainVT->Present = &HkPresent;

		g_SwapChainTables.insert(swapChainVT);
	}
}