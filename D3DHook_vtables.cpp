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
#define HOOK_FUNCTION(FP_, Name_) HookFunction(FP_, Hk##Name_, Real##Name_)

void UnhookFunction(auto& function, auto& backup)
requires std::same_as<std::remove_cvref_t<decltype(function)>, std::remove_cvref_t<decltype(backup)>>
{
	DWORD oldProtect;
	VirtualProtect(&function, sizeof(void*), PAGE_READWRITE, &oldProtect);
	function = backup;
	VirtualProtect(&function, sizeof(void*), oldProtect, &oldProtect);
}

#define DECLARE_HOOK(Type_, ShortTypeName_, Name_) \
std::decay_t<decltype(Type_##Vtbl::Name_)> Real##ShortTypeName_##Name_ = nullptr; \
HRESULT STDMETHODCALLTYPE Hk##ShortTypeName_##Name_

DECLARE_HOOK(IDXGISwapChain, SwapChain, Present)(
	IDXGISwapChain* This,
	UINT SyncInterval,
	UINT Flags)
{
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(This);
	auto returnValue = RealSwapChainPresent(This, SyncInterval, Flags);
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(This);
	return returnValue;
}

DECLARE_HOOK(IDXGISwapChain1, SwapChain1, Present1)(
	IDXGISwapChain1* This,
	UINT SyncInterval,
	UINT PresentFlags,
	const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::BeforeCall, GW2Load_PresentCallback>(Downcast(This));
	auto returnValue = RealSwapChain1Present1(This, SyncInterval, PresentFlags, pPresentParameters);
	InvokeAPIHooks<GW2Load_HookedFunction::Present, GW2Load_CallbackPoint::AfterCall, GW2Load_PresentCallback>(Downcast(This));
	return returnValue;
}

DECLARE_HOOK(IDXGISwapChain, SwapChain, ResizeBuffers)(
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

DECLARE_HOOK(IDXGISwapChain3, SwapChain3, ResizeBuffers1)(
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

DECLARE_HOOK(ID3D11Device, Device, CreateBuffer)(
	ID3D11Device* This,
	const D3D11_BUFFER_DESC* pDesc,
	const D3D11_SUBRESOURCE_DATA* pInitialData,
	ID3D11Buffer** ppBuffer)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreateBuffer, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreateBufferCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppBuffer);
	auto returnValue = RealDeviceCreateBuffer(This, pDesc, pInitialData, ppBuffer);
	InvokeAPIHooks<GW2Load_HookedFunction::CreateBuffer, GW2Load_CallbackPoint::AfterCall, GW2Load_CreateBufferCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppBuffer);
	return returnValue;
}

DECLARE_HOOK(ID3D11Device, Device, CreateTexture1D)(
	ID3D11Device* This,
	const D3D11_TEXTURE1D_DESC* pDesc,
	const D3D11_SUBRESOURCE_DATA* pInitialData,
	ID3D11Texture1D** ppTexture1D)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture1D, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreateTexture1DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture1D);
	auto returnValue = RealDeviceCreateTexture1D(This, pDesc, pInitialData, ppTexture1D);
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture1D, GW2Load_CallbackPoint::AfterCall, GW2Load_CreateTexture1DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture1D);
	return returnValue;
}

DECLARE_HOOK(ID3D11Device, Device, CreateTexture2D)(
	ID3D11Device* This,
	const D3D11_TEXTURE2D_DESC* pDesc,
	const D3D11_SUBRESOURCE_DATA* pInitialData,
	ID3D11Texture2D** ppTexture2D)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture2D, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreateTexture2DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture2D);
	auto returnValue = RealDeviceCreateTexture2D(This, pDesc, pInitialData, ppTexture2D);
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture2D, GW2Load_CallbackPoint::AfterCall, GW2Load_CreateTexture2DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture2D);
	return returnValue;
}

DECLARE_HOOK(ID3D11Device, Device, CreateTexture3D)(
	ID3D11Device* This,
	const D3D11_TEXTURE3D_DESC* pDesc,
	const D3D11_SUBRESOURCE_DATA* pInitialData,
	ID3D11Texture3D** ppTexture3D)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture3D, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreateTexture3DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture3D);
	auto returnValue = RealDeviceCreateTexture3D(This, pDesc, pInitialData, ppTexture3D);
	InvokeAPIHooks<GW2Load_HookedFunction::CreateTexture3D, GW2Load_CallbackPoint::AfterCall, GW2Load_CreateTexture3DCallback>(This, MutCast(pDesc), MutCast(pInitialData), ppTexture3D);
	return returnValue;
}

DECLARE_HOOK(ID3D11Device, Device, CreatePixelShader)(
	ID3D11Device* This,
	const void* pShaderBytecode,
	SIZE_T BytecodeLength,
	ID3D11ClassLinkage* pClassLinkage,
	ID3D11PixelShader** ppPixelShader)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreatePixelShader, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreatePixelShaderCallback>(This, MutCast(pShaderBytecode), BytecodeLength, pClassLinkage, ppPixelShader);
	auto returnValue = RealDeviceCreatePixelShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	InvokeAPIHooks<GW2Load_HookedFunction::CreatePixelShader, GW2Load_CallbackPoint::AfterCall, GW2Load_CreatePixelShaderCallback>(This, MutCast(pShaderBytecode), BytecodeLength, pClassLinkage, ppPixelShader);
	return returnValue;
}

DECLARE_HOOK(ID3D11Device, Device, CreateVertexShader)(
	ID3D11Device* This,
	const void* pShaderBytecode,
	SIZE_T BytecodeLength,
	ID3D11ClassLinkage* pClassLinkage,
	ID3D11VertexShader** ppVertexShader)
{
	InvokeAPIHooks<GW2Load_HookedFunction::CreateVertexShader, GW2Load_CallbackPoint::BeforeCall, GW2Load_CreateVertexShaderCallback>(This, MutCast(pShaderBytecode), BytecodeLength, pClassLinkage, ppVertexShader);
	auto returnValue = RealDeviceCreateVertexShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	InvokeAPIHooks<GW2Load_HookedFunction::CreateVertexShader, GW2Load_CallbackPoint::AfterCall, GW2Load_CreateVertexShaderCallback>(This, MutCast(pShaderBytecode), BytecodeLength, pClassLinkage, ppVertexShader);
	return returnValue;
}

DECLARE_HOOK(IDXGIFactory, Factory, CreateSwapChain)(
	IDXGIFactory* This,
	IUnknown* pDevice,
	DXGI_SWAP_CHAIN_DESC* pDesc,
	IDXGISwapChain** ppSwapChain)
{
	spdlog::debug("SwapChain initialization performed via IDXGIFactory::CreateSwapChain.");

	auto rval = RealFactoryCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (SUCCEEDED(rval) && ppSwapChain)
		InitializeSwapChain(*ppSwapChain);
	return rval;
}

DECLARE_HOOK(IDXGIFactory2, Factory2, CreateSwapChainForComposition)(
	IDXGIFactory2* This,
	IUnknown* pDevice,
	const DXGI_SWAP_CHAIN_DESC1* pDesc,
	IDXGIOutput* pRestrictToOutput,
	IDXGISwapChain1** ppSwapChain)
{
	spdlog::debug("SwapChain initialization performed via IDXGIFactory2::CreateSwapChainForComposition.");

	auto rval = RealFactory2CreateSwapChainForComposition(This, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	if (SUCCEEDED(rval) && ppSwapChain)
		InitializeSwapChain(Downcast(*ppSwapChain));
	return rval;
}

DECLARE_HOOK(IDXGIFactory2, Factory2, CreateSwapChainForCoreWindow)(
	IDXGIFactory2* This,
	IUnknown* pDevice,
	IUnknown* pWindow,
	const DXGI_SWAP_CHAIN_DESC1* pDesc,
	IDXGIOutput* pRestrictToOutput,
	IDXGISwapChain1** ppSwapChain)
{
	spdlog::debug("SwapChain initialization performed via IDXGIFactory2::CreateSwapChainForCoreWindow.");

	auto rval = RealFactory2CreateSwapChainForCoreWindow(This, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	if (SUCCEEDED(rval) && ppSwapChain)
		InitializeSwapChain(Downcast(*ppSwapChain));
	return rval;
}

DECLARE_HOOK(IDXGIFactory2, Factory2, CreateSwapChainForHwnd)(
	IDXGIFactory2* This,
	IUnknown* pDevice,
	HWND hWnd,
	const DXGI_SWAP_CHAIN_DESC1* pDesc,
	const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
	IDXGIOutput* pRestrictToOutput,
	IDXGISwapChain1** ppSwapChain)
{
	spdlog::debug("SwapChain initialization performed via IDXGIFactory2::CreateSwapChainForHwnd.");

	auto rval = RealFactory2CreateSwapChainForHwnd(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (SUCCEEDED(rval) && ppSwapChain)
		InitializeSwapChain(Downcast(*ppSwapChain));
	return rval;
}

using SwapChainVirtualTable = std::variant<IDXGISwapChainVtbl*, IDXGISwapChain1Vtbl*, IDXGISwapChain3Vtbl*>;
std::vector<SwapChainVirtualTable> g_SwapChainTables;
std::vector<ID3D11DeviceVtbl*> g_DeviceTables;

void InitializeSwapChain(IDXGISwapChain* sc)
{
	OverwriteSwapChainVTables(sc);
	InitializeD3DObjects(sc);
}

void OverwriteSwapChainVTables(IDXGISwapChain* swapChain)
{
	if (!swapChain)
		return;

	spdlog::debug("Attempting to overwrite device/context/swapchain vtables...");
	auto [device, deviceContext] = GetDeviceFromSwapChain(swapChain);

	auto* swapChainVT = swapChain->lpVtbl;
	if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChainVT)) == g_SwapChainTables.end())
	{
		spdlog::debug("SwapChain vtable is new: hooking!");
		HOOK_FUNCTION(swapChainVT->Present, SwapChainPresent);
		HOOK_FUNCTION(swapChainVT->ResizeBuffers, SwapChainResizeBuffers);

		g_SwapChainTables.push_back(swapChainVT);

		auto* sc1 = GetSwapChain1(swapChain);
		if (sc1)
		{
			spdlog::debug("SwapChain1 is available, checking...");
			auto* swapChain1VT = sc1->lpVtbl;
			if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChain1VT)) == g_SwapChainTables.end())
			{
				spdlog::debug("SwapChain1 vtable is new: hooking!");
				HOOK_FUNCTION(swapChain1VT->Present1, SwapChain1Present1);
				swapChain1VT->Release(sc1);

				g_SwapChainTables.push_back(swapChain1VT);
			}
		}

		auto* sc3 = GetSwapChain3(swapChain);
		if (sc3)
		{
			spdlog::debug("SwapChain3 is available, checking...");
			auto* swapChain3VT = sc3->lpVtbl;
			if (std::ranges::find(g_SwapChainTables, SwapChainVirtualTable(swapChain3VT)) == g_SwapChainTables.end())
			{
				spdlog::debug("SwapChain3 vtable is new: hooking!");
				HOOK_FUNCTION(swapChain3VT->ResizeBuffers1, SwapChain3ResizeBuffers1);
				swapChain3VT->Release(sc3);

				g_SwapChainTables.push_back(swapChain3VT);
			}
		}
	}

	auto* deviceVT = device->lpVtbl;
	if (std::ranges::find(g_DeviceTables, deviceVT) == g_DeviceTables.end())
	{
		spdlog::debug("Device vtable is new: hooking!");

		HOOK_FUNCTION(deviceVT->CreateBuffer, DeviceCreateBuffer);
		HOOK_FUNCTION(deviceVT->CreateTexture1D, DeviceCreateTexture1D);
		HOOK_FUNCTION(deviceVT->CreateTexture2D, DeviceCreateTexture2D);
		HOOK_FUNCTION(deviceVT->CreateTexture3D, DeviceCreateTexture3D);
		HOOK_FUNCTION(deviceVT->CreatePixelShader, DeviceCreatePixelShader);
		HOOK_FUNCTION(deviceVT->CreateVertexShader, DeviceCreateVertexShader);
	}

	auto* contextVT = deviceContext->lpVtbl;
}

using DXGIFactoryVirtualTable = std::variant<IDXGIFactoryVtbl*, IDXGIFactory2Vtbl*>;
std::vector<DXGIFactoryVirtualTable> g_DXGIFactoryTables;

void OverwriteDXGIFactoryVTables(IDXGIFactory* factory)
{
	spdlog::debug("Attempting to overwrite DXGI factory vtables...");

	auto* factoryVT = factory->lpVtbl;

	if (std::ranges::find(g_DXGIFactoryTables, DXGIFactoryVirtualTable(factoryVT)) == g_DXGIFactoryTables.end())
	{
		spdlog::debug("DXGI factory vtable is new: hooking!");

		HOOK_FUNCTION(factoryVT->CreateSwapChain, FactoryCreateSwapChain);

		g_DXGIFactoryTables.push_back(factoryVT);

		auto* f2 = GetFactory2(factory);
		if (f2)
		{
			spdlog::debug("DXGI factory 2 is available, checking...");
			auto* factory2VT = f2->lpVtbl;

			if (std::ranges::find(g_DXGIFactoryTables, DXGIFactoryVirtualTable(factory2VT)) == g_DXGIFactoryTables.end())
			{
				spdlog::debug("DXGI factory 2 vtable is new: hooking!");

				HOOK_FUNCTION(factory2VT->CreateSwapChainForComposition, Factory2CreateSwapChainForComposition);
				HOOK_FUNCTION(factory2VT->CreateSwapChainForCoreWindow, Factory2CreateSwapChainForCoreWindow);
				HOOK_FUNCTION(factory2VT->CreateSwapChainForHwnd, Factory2CreateSwapChainForHwnd);

				g_DXGIFactoryTables.push_back(factory2VT);

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

	for (auto& factory : g_DXGIFactoryTables)
	{
		if (auto* p = std::get_if<IDXGIFactoryVtbl*>(&factory); p)
		{
			auto* factoryVT = *p;
			UnhookFunction(factoryVT->CreateSwapChain, RealFactoryCreateSwapChain);
		}
		if (auto* p = std::get_if<IDXGIFactory2Vtbl*>(&factory); p)
		{
			auto* factory2VT = *p;
			UnhookFunction(factory2VT->CreateSwapChainForComposition, RealFactory2CreateSwapChainForComposition);
			UnhookFunction(factory2VT->CreateSwapChainForCoreWindow, RealFactory2CreateSwapChainForCoreWindow);
			UnhookFunction(factory2VT->CreateSwapChainForHwnd, RealFactory2CreateSwapChainForHwnd);
		}
	}
	g_DXGIFactoryTables.clear();
}