module;
#include "framework.h"

#define CINTERFACE
#define D3D11_NO_HELPERS
#include <d3d11.h>

export module VTables;

import std;

void HookFunction(auto& function, const auto& hook, auto& backup)
requires std::same_as<std::remove_cvref_t<decltype(function)>, std::remove_cvref_t<std::add_pointer_t<decltype(hook)>>>
	  && std::same_as<std::remove_cvref_t<decltype(function)>, std::remove_cvref_t<decltype(backup)>>
{
	DWORD oldProtect;
	VirtualProtect(&function, sizeof(void*), PAGE_READWRITE, &oldProtect);
	backup = function;
	function = hook;
	VirtualProtect(&function, sizeof(void*), oldProtect, &oldProtect);
}

void UnhookFunction(auto& function, auto& backup)
requires std::same_as<std::remove_cvref_t<decltype(function)>, std::remove_cvref_t<decltype(backup)>>
{
	DWORD oldProtect;
	VirtualProtect(&function, sizeof(void*), PAGE_READWRITE, &oldProtect);
	function = backup;
	VirtualProtect(&function, sizeof(void*), oldProtect, &oldProtect);
}

IDXGISwapChain* g_SwapChain;

decltype(IDXGISwapChainVtbl::AddRef) RealSwapChainAddRef = nullptr;
ULONG STDMETHODCALLTYPE HkSwapChainAddRef(
	IDXGISwapChain* This
)
{
	g_SwapChain = This;
	//UnhookFunction(This->lpVtbl->AddRef, RealSwapChainAddRef);
	return RealSwapChainAddRef(This);
}

decltype(IDXGISwapChainVtbl::Release) RealSwapChainRelease = nullptr;
ULONG STDMETHODCALLTYPE HkSwapChainRelease(
	IDXGISwapChain* This
)
{
	return RealSwapChainRelease(This);
}

decltype(IDXGISwapChainVtbl::Present) RealSwapChainPresent = nullptr;
HRESULT STDMETHODCALLTYPE HkSwapChainPresent(
	IDXGISwapChain* This,
	UINT SyncInterval,
	UINT Flags)
{
	return RealSwapChainPresent(This, SyncInterval, Flags);
}

std::set<IDXGISwapChainVtbl*> g_SwapChainTables;
std::set<ID3D11DeviceVtbl*> g_DeviceTables;
std::set<ID3D11DeviceContextVtbl*> g_DeviceContextTables;

export void OverwriteVTables(void* sc, void* dev, void* ctx)
{
	auto* swapChainVT = reinterpret_cast<IDXGISwapChain*>(sc)->lpVtbl;
	auto* deviceVT = reinterpret_cast<ID3D11Device*>(dev)->lpVtbl;
	auto* contextVT = reinterpret_cast<ID3D11DeviceContext*>(ctx)->lpVtbl;

	if (!g_SwapChainTables.contains(swapChainVT))
	{
		HookFunction(swapChainVT->Present, HkSwapChainPresent, RealSwapChainPresent);
		HookFunction(swapChainVT->AddRef, HkSwapChainAddRef, RealSwapChainAddRef);
		HookFunction(swapChainVT->Release, HkSwapChainRelease, RealSwapChainRelease);

		g_SwapChainTables.insert(swapChainVT);
	}
}