#define CINTERFACE
#define D3D11_NO_HELPERS

#include "D3DHook.h"
#include <variant>
#include "Utils.h"

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

decltype(IDXGISwapChainVtbl::Present) RealSwapChainPresent = nullptr;
HRESULT STDMETHODCALLTYPE HkSwapChainPresent(
	IDXGISwapChain* This,
	UINT SyncInterval,
	UINT Flags)
{
	InitializeD3DObjects(This);
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(This);
	auto returnValue = RealSwapChainPresent(This, SyncInterval, Flags);
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(This);
	return returnValue;
}

decltype(IDXGISwapChain1Vtbl::Present1) RealSwapChain1Present1 = nullptr;
HRESULT STDMETHODCALLTYPE HkSwapChain1Present1(
	IDXGISwapChain1* This,
	UINT SyncInterval,
	UINT PresentFlags,
	const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
	InitializeD3DObjects(reinterpret_cast<IDXGISwapChain*>(This));
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(Downcast(This));
	auto returnValue = RealSwapChain1Present1(This, SyncInterval, PresentFlags, pPresentParameters);
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(Downcast(This));
	return returnValue;
}

decltype(IDXGISwapChainVtbl::ResizeBuffers) RealSwapChainResizeBuffers = nullptr;
HRESULT STDMETHODCALLTYPE HkSwapChainResizeBuffers(
	IDXGISwapChain* This,
	UINT BufferCount,
	UINT Width,
	UINT Height,
	DXGI_FORMAT NewFormat,
	UINT SwapChainFlags)
{
	InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::BeforeCall, GW2Load_ResizeBuffersCallback>(This, Width, Height, NewFormat);
	auto returnValue = RealSwapChainResizeBuffers(This, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::AfterCall, GW2Load_ResizeBuffersCallback>(This, Width, Height, NewFormat);
	return returnValue;
}

decltype(IDXGISwapChain3Vtbl::ResizeBuffers1) RealSwapChain3ResizeBuffers1 = nullptr;
HRESULT STDMETHODCALLTYPE HkSwapChain3ResizeBuffers1(
	IDXGISwapChain3* This,
	UINT BufferCount,
	UINT Width,
	UINT Height,
	DXGI_FORMAT Format,
	UINT SwapChainFlags,
	const UINT* pCreationNodeMask,
	IUnknown* const* ppPresentQueue)
{
	InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::BeforeCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, Format);
	auto returnValue = RealSwapChain3ResizeBuffers1(This, BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, ppPresentQueue);
	InvokeAPIHooks<GW2Load_HookedFunction::ResizeBuffers, GW2Load_CallbackPoint::AfterCall, GW2Load_ResizeBuffersCallback>(Downcast(This), Width, Height, Format);
	return returnValue;
}

using SwapChainVirtualTable = std::variant<IDXGISwapChainVtbl*, IDXGISwapChain1Vtbl*, IDXGISwapChain3Vtbl*>;
std::vector<SwapChainVirtualTable> g_SwapChainTables;
//std::vector<ID3D11DeviceVtbl*> g_DeviceTables;
//std::vector<ID3D11DeviceContextVtbl*> g_DeviceContextTables;

void OverwriteVTables(void* sc, void* dev, void* ctx)
{
	spdlog::debug("Attempting to overwrite vtables...");

	auto* swapChainVT = reinterpret_cast<IDXGISwapChain*>(sc)->lpVtbl;
	auto* deviceVT = reinterpret_cast<ID3D11Device*>(dev)->lpVtbl;
	auto* contextVT = reinterpret_cast<ID3D11DeviceContext*>(ctx)->lpVtbl;

	if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChainVT)) == g_SwapChainTables.end())
	{
		spdlog::debug("SwapChain vtable is new: hooking!");
		HookFunction(swapChainVT->Present, HkSwapChainPresent, RealSwapChainPresent);
		HookFunction(swapChainVT->ResizeBuffers, HkSwapChainResizeBuffers, RealSwapChainResizeBuffers);

		g_SwapChainTables.push_back(swapChainVT);

		auto* sc1 = GetSwapChain1(reinterpret_cast<IDXGISwapChain*>(sc));
		if (sc1)
		{
			spdlog::debug("SwapChain1 is available, checking...");
			auto* swapChain1VT = sc1->lpVtbl;
			if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChain1VT)) == g_SwapChainTables.end())
			{
				spdlog::debug("SwapChain1 vtable is new: hooking!");
				HookFunction(swapChain1VT->Present1, HkSwapChain1Present1, RealSwapChain1Present1);
				swapChain1VT->Release(sc1);

				g_SwapChainTables.push_back(swapChain1VT);
			}
		}

		auto* sc3 = GetSwapChain3(reinterpret_cast<IDXGISwapChain*>(sc));
		if (sc3)
		{
			spdlog::debug("SwapChain3 is available, checking...");
			auto* swapChain3VT = sc3->lpVtbl;
			if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChain3VT)) == g_SwapChainTables.end())
			{
				spdlog::debug("SwapChain3 vtable is new: hooking!");
				HookFunction(swapChain3VT->ResizeBuffers1, HkSwapChain3ResizeBuffers1, RealSwapChain3ResizeBuffers1);
				swapChain3VT->Release(sc3);

				g_SwapChainTables.push_back(swapChain3VT);
			}
		}

	}
}

void RestoreVTables()
{
	spdlog::debug("Restoring all vtables...");

	for (auto& vt : g_SwapChainTables)
	{
		if (auto* p = std::get_if<IDXGISwapChainVtbl*>(&vt); p)
		{
			auto* swapChainVT = *p;
			UnhookFunction(swapChainVT->Present, RealSwapChainPresent);
			UnhookFunction(swapChainVT->ResizeBuffers, RealSwapChainResizeBuffers);
		}

		if (auto* p = std::get_if<IDXGISwapChain1Vtbl*>(&vt); p)
		{
			auto* swapChain1VT = *p;
			UnhookFunction(swapChain1VT->Present1, RealSwapChain1Present1);
		}

		if (auto* p = std::get_if<IDXGISwapChain3Vtbl*>(&vt); p)
		{
			auto* swapChain3VT = *p;
			UnhookFunction(swapChain3VT->ResizeBuffers1, RealSwapChain3ResizeBuffers1);
		}
	}
	g_SwapChainTables.clear();
}